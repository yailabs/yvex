#!/usr/bin/env python3
"""Reproduce an external reference-only GGUF projection, never a YVEX artifact.

The immutable recipe owns aliases/exclusions, not this generic adapter. A pinned
independent GGUF reader supplies qtype geometry. Payload is copied exactly except
for explicitly requested, lossless BF16 bit embedding. No quantization occurs.
"""
import argparse
import hashlib
import importlib
import operator
from pathlib import Path
import struct
import sys

import qualification_reference as independent
import qualification_run as measurement


def widen(raw):
    if len(raw) % 2:
        raise ValueError("partial BF16 element")
    result = bytearray(len(raw) * 2)
    result[2::4], result[3::4] = raw[::2], raw[1::2]
    return result


def aligned(value, alignment):
    # Reader extents may be NumPy scalars: perform all file geometry in Python
    # integers, never in the reader's possibly 32-bit scalar arithmetic.
    value, alignment = operator.index(value), operator.index(alignment)
    if value < 0 or alignment <= 0 or alignment & (alignment - 1):
        raise ValueError("invalid GGUF alignment geometry")
    return (value + alignment - 1) // alignment * alignment


def projection_recipe(recipe):
    if recipe.get("schema") != "yvex.qualification.gguf-reference-projection.v1":
        raise ValueError("unsupported reference projection recipe")
    for key in ("source_sha256", "expected_output_sha256", "reader_revision", "reader_tree"):
        value = recipe.get(key)
        length = 40 if key.startswith("reader_") else 64
        if not isinstance(value, str) or len(value) != length or any(c not in "0123456789abcdef" for c in value):
            raise ValueError("invalid projection identity: " + key)
    if not recipe.get("limitations") or type(recipe.get("widen_bf16")) is not bool:
        raise ValueError("reference transformation policy and non-claims required")
    aliases = recipe.get("metadata_aliases")
    prefixes = recipe.get("exclude_prefixes")
    if (not isinstance(aliases, dict) or any(not isinstance(k, str) or not k or not isinstance(v, str) or not v
                                           for k, v in aliases.items())
            or len(set(aliases.values())) != len(aliases)
            or not isinstance(prefixes, list) or any(not isinstance(p, str) or not p for p in prefixes)):
        raise ValueError("invalid metadata/tensor projection")
    for key in ("expected_source_tensors", "expected_target_tensors", "expected_widened_tensors"):
        if type(recipe.get(key)) is not int or recipe[key] < 0:
            raise ValueError("invalid reference tensor population")
    return recipe


def copy_tensor(src, dst, tensor, widening):
    src.seek(int(tensor.data_offset))
    remaining = int(tensor.n_bytes)
    before, after = hashlib.sha256(), hashlib.sha256()
    while remaining:
        raw = src.read(min(remaining, 4 * 1024 * 1024))
        if not raw:
            raise ValueError("truncated source tensor")
        encoded = widen(raw) if widening else raw
        before.update(raw); after.update(encoded)
        dst.write(encoded)
        remaining -= len(raw)
    return before.hexdigest(), after.hexdigest()


def project(source, destination, recipe_path, reader_checkout):
    recipe = projection_recipe(independent.read(recipe_path))
    if destination.resolve().is_relative_to(measurement.ROOT) or destination.exists():
        raise ValueError("exclusive reference destination must remain outside Git")
    state = independent.source_state(reader_checkout)
    if (state["status"] or state["revision"] != recipe["reader_revision"]
            or state["tree"] != recipe["reader_tree"] or state["repository"] != recipe["reader_repository"]):
        raise ValueError("GGUF reader is not the exact clean independent source")
    if measurement.digest(source) != recipe["source_sha256"]:
        raise ValueError("reference input artifact differs")
    package = reader_checkout.resolve() / "gguf-py"
    sys.path.insert(0, str(package))
    gguf = importlib.import_module("gguf")
    if not Path(gguf.__file__).resolve().is_relative_to(package):
        raise ValueError("GGUF reader imported from another installation")
    reader = gguf.GGUFReader(source)
    if reader.endianess != gguf.GGUFEndian.LITTLE or reader.fields["GGUF.version"].contents() != 3:
        raise ValueError("reference projection admits little-endian GGUF v3 only")
    aliases = recipe["metadata_aliases"]
    metadata = [f for f in reader.fields.values() if f.offset >= 24]
    keys = {f.name for f in metadata}
    if not set(aliases) <= keys or set(aliases.values()) & keys:
        raise ValueError("metadata alias missing/colliding")
    tensors = [t for t in reader.tensors if not any(t.name.startswith(p) for p in recipe["exclude_prefixes"])]
    widened = [t for t in tensors if recipe["widen_bf16"] and t.tensor_type == gguf.GGMLQuantizationType.BF16]
    if (len(reader.tensors), len(tensors), len(widened)) != (
            recipe["expected_source_tensors"], recipe["expected_target_tensors"], recipe["expected_widened_tensors"]):
        raise ValueError("reference tensor population differs from recipe")
    # Exhaustive embedding oracle independent of vectorized byte placement.
    patterns = b"".join(struct.pack("<H", n) for n in range(65536))
    if widen(patterns) != b"".join(struct.pack("<I", n << 16) for n in range(65536)):
        raise ValueError("BF16 bit embedding oracle failed")
    alignment = int(reader.alignment)
    align = lambda n: aligned(n, alignment)
    records = []
    with source.open("rb") as src, destination.open("xb") as dst:
        dst.write(struct.pack("<4sIQQ", b"GGUF", 3, len(tensors), len(metadata)))
        for field in metadata:
            # Preserve the type/value bytes, including tokenizer arrays, exactly.
            count = sum(int(part.nbytes) for part in field.parts)
            src.seek(int(field.offset))
            raw = src.read(count)
            old = field.name.encode()
            if raw[:8 + len(old)] != struct.pack("<Q", len(old)) + old:
                raise ValueError("independent metadata extent disagrees")
            new = aliases.get(field.name, field.name).encode()
            dst.write(struct.pack("<Q", len(new)) + new + raw[8 + len(old):])
        offset = 0
        for tensor in tensors:
            shape = [int(d) for d in tensor.shape]
            widened_tensor = recipe["widen_bf16"] and tensor.tensor_type == gguf.GGMLQuantizationType.BF16
            qtype = gguf.GGMLQuantizationType.F32 if widened_tensor else tensor.tensor_type
            size = int(tensor.n_bytes) * (2 if widened_tensor else 1)
            name = tensor.name.encode()
            dst.write(struct.pack("<Q", len(name)) + name + struct.pack("<I", len(shape)) +
                      struct.pack("<" + "Q" * len(shape), *shape) + struct.pack("<IQ", int(qtype), offset))
            records.append(dict(name=tensor.name, source_qtype=int(tensor.tensor_type), qtype=int(qtype),
                                source_bytes=int(tensor.n_bytes), bytes=size, offset=offset, widened=widened_tensor))
            offset += align(size)
        dst.write(bytes(align(dst.tell()) - dst.tell()))
        data_start = dst.tell()
        for tensor, record in zip(tensors, records):
            if dst.tell() != data_start + record["offset"]:
                raise ValueError("dense reference tensor offset differs")
            before, after = copy_tensor(src, dst, tensor, record["widened"])
            record.update(source_sha256=before, tensor_sha256=after)
            dst.write(bytes(align(record["bytes"]) - record["bytes"]))
    with source.open("rb") as src, destination.open("rb") as dst:
        for tensor, record in zip(tensors, records):
            src.seek(int(tensor.data_offset)); dst.seek(data_start + record["offset"])
            remaining = int(tensor.n_bytes)
            while remaining:
                raw = src.read(min(remaining, 4 * 1024 * 1024))
                expected = widen(raw) if record["widened"] else raw
                if not raw or dst.read(len(expected)) != expected:
                    raise ValueError("reference tensor readback differs")
                remaining -= len(raw)
    output_identity = measurement.digest(destination)
    if output_identity != recipe["expected_output_sha256"] or independent.source_state(reader_checkout) != state:
        raise ValueError("reference projection or reader identity changed")
    receipt = dict(schema="yvex.qualification.gguf-reference-receipt.v1", recipe_identity=measurement.canonical(recipe),
                   source_container_sha256=recipe["source_sha256"], container_sha256=output_identity,
                   reader=state, adapter_sha256=measurement.digest(Path(__file__)), tensors=records,
                   limitations=recipe["limitations"])
    independent.write(destination.with_suffix(".json"), receipt)
    print(measurement.canonical(receipt))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--reader-checkout", type=Path, required=True)
    args = parser.parse_args()
    project(args.source, args.output, args.recipe, args.reader_checkout)


if __name__ == "__main__":
    main()
