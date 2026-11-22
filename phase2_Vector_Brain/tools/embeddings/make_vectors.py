
import struct
from sentence_transformers import SentenceTransformer

with open("sentences.txt", "r", encoding="utf-8") as f:
    my_list = [line.strip() for line in f]

print(len(my_list))

model = SentenceTransformer("all-MiniLM-L6-v2")
vectors = model.encode(my_list)
print(vectors.shape)
print(vectors.dtype)
print(vectors[0][:20])

count, dim = vectors.shape

with open("../../data/vectors.dat", "wb") as out:
    # Write the 8-byte header (count and dimension)
    out.write(struct.pack("<III", dim, count, 0))
    
    # Pad the rest of the 4096-byte page with zeros
    out.write(bytes(4096 - 12))
    
    # Dump the raw float32 bytes directly to disk
    vectors.astype('float32').tofile(out)