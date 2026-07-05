import os
import re
from io import StringIO
from collections import Counter

import numpy as np
import pandas as pd

WINDOW_SAMPLES = 180
AXIS_STD_FLOOR_G = 0.05

# labels for letters. keywords must be in the filename
LABELS = {
    "o_letter": [1, 0, 0],
    "z_letter": [0, 1, 0],
    "i_letter": [0, 0, 1],
}

NUMERIC_LINE = re.compile(
    r"^\s*-?\d+(?:\.\d+)?\s*,\s*-?\d+(?:\.\d+)?\s*,\s*-?\d+(?:\.\d+)?\s*$"
)

# extracts the one-hot label by checking if the filename contains the letter keywords
def label_from_filename(path: str):
    name = os.path.basename(path).lower()
    for key, one_hot in LABELS.items():
        if key in name:
            return one_hot
    return None

# reads standard csvs and blocks copied from esp32 serial output
def read_sensor_csv(path: str) -> pd.DataFrame:
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        lines = [line.strip() for line in f if line.strip()]

    useful = []
    header_seen = False
    for line in lines:
        if line.startswith("Accel_X"):
            useful.append("Accel_X,Accel_Y,Accel_Z")
            header_seen = True
        elif NUMERIC_LINE.match(line):
            useful.append(line)

    if not useful:
        raise ValueError("accel_x, accel_y, accel_z data not found in file")

    if not header_seen:
        useful.insert(0, "Accel_X,Accel_Y,Accel_Z")

    df = pd.read_csv(StringIO("\n".join(useful)))
    return df[["Accel_X", "Accel_Y", "Accel_Z"]]

# converts centi-g to g, applies per-window and per-axis mean subtraction and std normalization
def preprocess_window_cg(window_cg: np.ndarray) -> np.ndarray:
    w = window_cg.astype(np.float32) / 100.0
    mean = w.mean(axis=0, keepdims=True)
    std = w.std(axis=0, keepdims=True)
    std = np.maximum(std, AXIS_STD_FLOOR_G)
    w = (w - mean) / std
    return w.reshape(-1).astype(np.float32)

# main execution block
def main():
    print("relative preprocessing started...")
    print(f"WINDOW_SAMPLES={WINDOW_SAMPLES}, AXIS_STD_FLOOR_G={AXIS_STD_FLOOR_G}")

    X_list = []
    y_list = []
    used_files = []

    csv_files = []
    # walk through directory to find all csv files
    for root, _, files in os.walk("."):
        for file in files:
            if file.lower().endswith(".csv"):
                csv_files.append(os.path.join(root, file))

    # process each valid csv file
    for path in sorted(csv_files):
        y = label_from_filename(path)
        if y is None:
            continue

        try:
            df = read_sensor_csv(path)
            # check if file has enough samples
            if len(df) < WINDOW_SAMPLES:
                print(f"skipped: {path} ({len(df)} rows, need at least {WINDOW_SAMPLES})")
                continue

            # extract window and preprocess
            window_cg = df[["Accel_X", "Accel_Y", "Accel_Z"]].iloc[:WINDOW_SAMPLES].values
            X_list.append(preprocess_window_cg(window_cg))
            y_list.append(y)
            used_files.append(path)
        except Exception as exc:
            print(f"skipped: {path} -> {exc}")

    if not X_list:
        raise SystemExit("no training csv found. filenames must contain o_letter, z_letter, or i_letter.")

    X = np.vstack(X_list).astype(np.float32)
    y = np.array(y_list, dtype=np.float32)

    # export data arrays
    np.save("X_data.npy", X)
    np.save("y_tags.npy", y)

    label_names = ["O", "Z", "I"]
    counts = Counter(np.argmax(y, axis=1))

    # print final summary
    print(f"processed files: {len(used_files)}")
    print(f"X_data.npy: {X.shape}")
    print(f"y_tags.npy: {y.shape}")
    print("class counts:")
    for idx, name in enumerate(label_names):
        print(f"  {name}: {counts.get(idx, 0)}")
    print("note: this x data is already relative normalized. use your training script now.")

if __name__ == "__main__":
    main()