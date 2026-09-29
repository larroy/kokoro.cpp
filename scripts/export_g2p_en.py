"""Convert g2p_en's checkpoint20.npz into the binary format read by NeuralG2P.cpp.

The checkpoint comes from https://github.com/Kyubyong/g2p (Apache-2.0):
    https://github.com/Kyubyong/g2p/raw/master/g2p_en/checkpoint20.npz

Standard library only (no numpy needed).

Output format (little-endian):
    b"G2PE", u32 version (1), u32 tensor count
    per tensor: u32 name length, name bytes (utf-8), u32 ndim, u32 dims[ndim], f32 data
"""
import ast
import struct
import sys
import zipfile

TENSORS = [
    "enc_emb", "enc_w_ih", "enc_w_hh", "enc_b_ih", "enc_b_hh",
    "dec_emb", "dec_w_ih", "dec_w_hh", "dec_b_ih", "dec_b_hh",
    "fc_w", "fc_b",
]


def read_npy(data):
    if data[:6] != b"\x93NUMPY":
        raise ValueError("not an .npy payload")
    major = data[6]
    if major == 1:
        header_len = struct.unpack("<H", data[8:10])[0]
        offset = 10
    else:
        header_len = struct.unpack("<I", data[8:12])[0]
        offset = 12
    header = ast.literal_eval(data[offset:offset + header_len].decode("latin1"))
    if header["descr"] != "<f4" or header["fortran_order"]:
        raise ValueError(f"unsupported array layout: {header}")
    shape = tuple(header["shape"])
    payload = data[offset + header_len:]
    count = 1
    for d in shape:
        count *= d
    if len(payload) != count * 4:
        raise ValueError(f"payload size mismatch for shape {shape}")
    return shape, payload


def export(npz_path, output_path):
    with zipfile.ZipFile(npz_path) as z, open(output_path, "wb") as out:
        out.write(b"G2PE")
        out.write(struct.pack("<II", 1, len(TENSORS)))
        for name in TENSORS:
            shape, payload = read_npy(z.read(name + ".npy"))
            name_bytes = name.encode("utf-8")
            out.write(struct.pack("<I", len(name_bytes)))
            out.write(name_bytes)
            out.write(struct.pack("<I", len(shape)))
            out.write(struct.pack(f"<{len(shape)}I", *shape))
            out.write(payload)
            print(f"  {name}: {shape}")
    print(f"Wrote {output_path}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python export_g2p_en.py <checkpoint20.npz> <output g2p_en.weights>")
        sys.exit(1)
    export(sys.argv[1], sys.argv[2])
