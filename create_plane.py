
with open("Resources/models/ocean.obj", "w") as f:
    f.write("o Ocean\n")
    s = 10000.0
    f.write(f"v {-s} 0.0 {-s}\n")
    f.write(f"v {s} 0.0 {-s}\n")
    f.write(f"v {-s} 0.0 {s}\n")
    f.write(f"v {s} 0.0 {s}\n")
    f.write("vt 0.0 0.0\n")
    f.write("vt 100.0 0.0\n")
    f.write("vt 0.0 100.0\n")
    f.write("vt 100.0 100.0\n")
    f.write("vn 0.0 1.0 0.0\n")
    f.write("f 1/1/1 2/2/1 4/4/1 3/3/1\n")

