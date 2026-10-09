#!/usr/bin/env python3
"""Independent DeepSeek tokenizer/prompt oracle; production never imports this module."""
import importlib.util
import hashlib
import json
import pathlib
import subprocess
import sys
import argparse

import tokenizers
from tokenizers import Tokenizer


def native_request(native, artifact, binding, operation, *arguments):
    return subprocess.check_output(
        [str(native), str(artifact), str(binding), operation, *arguments], text=True)


def ids_from_native(native, artifact, binding, text):
    return [int(value) for value in
            native_request(native, artifact, binding, "--reference-encode", text).split()]


def text_from_native(native, artifact, binding, token_ids):
    return native_request(native, artifact, binding, "--reference-decode",
                          ",".join(map(str, token_ids)))


def prompt_from_native(native, artifact, binding, mode, messages):
    arguments = [mode]
    for message in messages:
        arguments.extend([message["role"], message["content"],
                          message.get("reasoning_content", "")])
    return native_request(native, artifact, binding, "--reference-render", *arguments)


def official_vectors(source, artifact, oracle, native, binding, authority_key):
    manifest = json.loads((pathlib.Path(__file__).parents[1] / "vectors/manifest.json").read_text())
    authority = manifest[authority_key]
    for name, expected in authority["files"].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise AssertionError(f"official vector identity mismatch: {name}")
    # Run the immutable upstream tests without modifying/vendoring their source.
    upstream = subprocess.run(
        [sys.executable, "-B", "-c",
         "import pathlib, runpy, sys, types; "
         "p=pathlib.Path('encoding_dsv4.py'); m=types.ModuleType('encoding_dsv4'); "
         "m.__file__=str(p); exec(compile(p.read_bytes(),str(p),'exec'),m.__dict__); "
         "sys.modules['encoding_dsv4']=m; "
         "runpy.run_path('test_encoding_dsv4.py',run_name='__main__')"], cwd=source / "encoding",
        check=True, capture_output=True, text=True,
    )
    if upstream.stdout.count("[PASS]") != 4:
        raise AssertionError("official upstream case count changed")
    for case in range(1, 5):
        gold = (source / f"encoding/tests/test_output_{case}.txt").read_text()
        expected = oracle.encode(gold, add_special_tokens=False).ids
        if ids_from_native(native, artifact, binding, gold) != expected:
            raise AssertionError(f"official case {case}: native BPE differs")
        if text_from_native(native, artifact, binding, expected) != gold:
            raise AssertionError(f"official case {case}: native decode differs")
    # The serving contract is a request ending in user/tool, not a completed
    # transcript ending in assistant. Use the request messages from case 2 under
    # the selected high policy and compare with upstream construction. This is
    # not full-vector transcript equivalence or the default-effort gold prefix.
    # The full four immutable gold strings above qualify BPE only.
    messages = json.loads((source / "encoding/tests/test_input_2.json").read_text())
    spec = importlib.util.spec_from_file_location("official_encoding", source / "encoding/encoding_dsv4.py")
    module = importlib.util.module_from_spec(spec)
    # Never mutate the authenticated source inventory or execute cached bytecode.
    exec(compile((source / "encoding/encoding_dsv4.py").read_bytes(),
                 str(source / "encoding/encoding_dsv4.py"), "exec"), module.__dict__)
    request = messages[:-1]
    rendered = prompt_from_native(native, artifact, binding, "thinking", request)
    expected = module.encode_messages(request, "thinking", reasoning_effort="high")
    if rendered != expected:
        raise AssertionError("official case 2 request prefix: typed native prompt bytes differ")
    print(f"official_source={authority['repository']} revision={authority['revision']} "
          "upstream_encoding_cases=4 native_bpe_cases=4 native_request_prefix_cases=1 "
          "native_request_reasoning=high "
          "full_native_transcript_projection=not-qualified "
          "native_tool_developer_reminder_projection=not-qualified full_model_logits=not-run")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "artifact", "native", "binding"):
        parser.add_argument(name, type=pathlib.Path)
    parser.add_argument("--authority", default="deepseek_official_encoding",
                        choices=("deepseek_official_encoding", "deepseek_0731_official_encoding"),
                        help="exact immutable checkpoint authority; no automatic fallback")
    args = parser.parse_args()
    source, artifact, native, binding = [getattr(args, key).resolve()
                                        for key in ("source", "artifact", "native", "binding")]
    if tokenizers.__version__ != "0.20.3":
        raise AssertionError(f"unexpected tokenizers version {tokenizers.__version__}")
    oracle = Tokenizer.from_file(str(source / "tokenizer.json"))
    official_vectors(source, artifact, oracle, native, binding, args.authority)
    corpus = [
        "", "hello world", "  repeated   spaces\nnext\tline", "café e\u0301",
        "😀🧠", "你好世界", "こんにちは世界", "Привет мир", "مرحبا بالعالم",
        "नमस्ते दुनिया", "1234567890", "<think>hello</think>",
        "<｜User｜>Hello<｜Assistant｜></think>",
    ]
    for text in corpus:
        expected = oracle.encode(text, add_special_tokens=False).ids
        actual = ids_from_native(native, artifact, binding, text)
        if actual != expected:
            raise AssertionError((text, expected, actual))
        if expected:
            decoded = text_from_native(native, artifact, binding, expected)
            expected_decoded = oracle.decode(expected, skip_special_tokens=False)
            if decoded != expected_decoded:
                raise AssertionError((expected, expected_decoded, decoded))
    module_path = source / "encoding" / "encoding_dsv4.py"
    spec = importlib.util.spec_from_file_location("encoding_dsv4", module_path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(module_path.read_bytes(), str(module_path), "exec"), module.__dict__)
    messages = [
        {"role": "system", "content": "policy"},
        {"role": "user", "content": "hi"},
        {"role": "assistant", "content": "ok"},
        {"role": "user", "content": "next"},
    ]
    expected_prompt = module.encode_messages(messages, "chat")
    rendered = prompt_from_native(native, artifact, binding, "chat", messages)
    if rendered != expected_prompt:
        raise AssertionError((expected_prompt, rendered))
    tool_messages = [
        {"role": "system", "content": "policy"},
        {"role": "user", "content": "call"},
        {"role": "assistant", "content": "working"},
        {"role": "tool", "content": "one"},
        {"role": "tool", "content": "two"},
    ]
    expected_tool_prompt = module.encode_messages(tool_messages, "chat")
    rendered = prompt_from_native(native, artifact, binding, "chat", tool_messages)
    if rendered != expected_tool_prompt:
        raise AssertionError((expected_tool_prompt, rendered))
    # Supplemental authored controls are not additional official vectors.
    for mode, effort in (("thinking", "high"), ("maximum", "max")):
        reasoning_messages = [{"role": "user", "content": "reason"}]
        expected_thinking = module.encode_messages(reasoning_messages, "thinking",
                                                   reasoning_effort=effort)
        rendered = prompt_from_native(native, artifact, binding, mode, reasoning_messages)
        if rendered != expected_thinking:
            raise AssertionError((mode, expected_thinking, rendered))
    print("tokenizer_reference=tokenizers-0.20.3 cases=13 prompt_cases=4 "
          "reasoning_modes=none,high,maximum "
          "encode_decode_parity=pass")


if __name__ == "__main__":
    main()
