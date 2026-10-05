#!/usr/bin/env python3
"""Authenticate the exact small checkpoint, execute its Jinja and compare native bytes/IDs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from jinja2.sandbox import ImmutableSandboxedEnvironment
from tokenizers import Tokenizer

CONFIG_SHA = "49e2b6e395f959f077f1e992b338919c0d4a9732fc6e613995e06557f843500c"
TOKENIZER_SHA = "5f9e4d4901a92b997e463c1f46055088b6cca5ca61a6522d1b9f64c4bb81cb42"
TEMPLATE_SHA = "273d8e0e683b885071fb17e08d71e5f2a5ddfb5309756181681de4f5a1822d80"


def reject(message):
    raise ValueError(message)


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: qwen_small_conversation.py SOURCE ARTIFACT BINDING NATIVE")
    source, artifact, binding, native = map(Path, sys.argv[1:])
    for name, sha in (("tokenizer_config.json", CONFIG_SHA), ("tokenizer.json", TOKENIZER_SHA)):
        assert hashlib.sha256((source / name).read_bytes()).hexdigest() == sha, name
    config = json.loads((source / "tokenizer_config.json").read_bytes())
    template_bytes = config["chat_template"].encode()
    assert hashlib.sha256(template_bytes).hexdigest() == TEMPLATE_SHA
    assert config["eos_token"] == "<|im_end|>" and config["pad_token"] == "<|endoftext|>"
    env = ImmutableSandboxedEnvironment(trim_blocks=True, lstrip_blocks=True)
    env.globals["raise_exception"] = reject
    env.filters["tojson"] = lambda value: json.dumps(value, ensure_ascii=False)
    template = env.from_string(config["chat_template"])
    tokenizer = Tokenizer.from_file(str(source / "tokenizer.json"))

    def call(mode, *args, ok=True):
        result = subprocess.run([str(native), str(artifact), str(binding), mode, *args], capture_output=True)
        assert (result.returncode == 0) == ok, result.stderr.decode()
        return result.stdout

    user = lambda text: {"role": "user", "content": text}
    assistant = lambda text, reasoning="": {"role": "assistant", "content": text, "reasoning_content": reasoning}
    cases = [
        [user("hello")],
        [user("My name is Ada. Reply with my name only.")],
        [user("My name is Ada. Reply with my name only."), assistant("Ada"),
         user("What is my name? Reply with my name only.")],
        [{"role": "system", "content": " policy "}, user(" ciao ")],
        [{"role": "system", "content": ""}, user("hello")],
        [user("first"), assistant("answer", "private thought"), user("second")],
        [user("你好 café 😀"), assistant("sì"), user("次の質問")],
        [user("call"), assistant("working"), {"role": "tool", "content": "one"}, {"role": "tool", "content": "two"}],
    ]
    proofs = []
    for messages in cases:
        for mode in ("chat", "thinking", "transcript"):
            expected = template.render(messages=messages, add_generation_prompt=mode != "transcript",
                                       enable_thinking=mode == "thinking").encode()
            args = [value for message in messages for value in
                    (message["role"], message["content"], message.get("reasoning_content", ""))]
            actual = call(mode, *args)
            assert actual == expected, (mode, messages, expected, actual)
            ids = tokenizer.encode(expected.decode(), add_special_tokens=False).ids
            actual_ids = list(map(int, call("encode", expected.decode()).split()))
            assert actual_ids == ids, (mode, messages, ids, actual_ids)
            proofs.append({"mode": mode, "messages": messages, "prompt": expected.decode(), "ids": ids,
                           "prompt_sha256": hashlib.sha256(expected).hexdigest()})
    # Source rejects missing user, misplaced system and unknown role. Native
    # contracts additionally reject orphan assistant/tool and invalid UTF-8.
    for messages in ([], [{"role": "system", "content": "policy"}],
                     [user("hi"), {"role": "system", "content": "late"}],
                     [{"role": "developer", "content": "unsupported"}],
                     [user("<tool_response>result</tool_response>")]):
        try:
            template.render(messages=messages, add_generation_prompt=True)
        except Exception as error:
            assert str(error)
        else:
            raise AssertionError(("source admitted malformed conversation", messages))
        args = [value for message in messages for value in (message["role"], message["content"], "")]
        call("chat", *args, ok=False)
    assert call("negative").strip() == b"authority_refusals=4"
    print(json.dumps({"schema": "yvex.evidence.qwen-small-conversation-reference.v1",
                      "source_revision": "2fc06364715b967f1860aea9cf38778875588b17",
                      "config_sha256": CONFIG_SHA, "template_sha256": TEMPLATE_SHA,
                      "prompt_token_cases": len(proofs), "malformed_cases": 5,
                      "authority_refusals": 4, "proofs": proofs}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
