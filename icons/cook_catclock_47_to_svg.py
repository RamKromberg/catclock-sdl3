import sys
import os
import cv2
import numpy as np

def trace_pixel_art_exact_outline(image_path):
    img = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
    if img is None:
        print(f"Error: Could not read image at {image_path}", file=sys.stderr)
        return ""

    # Extract the alpha channel mask safely if it exists
    if len(img.shape) == 3 and img.shape[2] == 4:
        alpha = img[:, :, 3]
    else:
        # Grayscale fallback: any non-black pixel belongs to the cat silhouette
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY) if len(img.shape) == 3 else img
        _, alpha = cv2.threshold(gray, 1, 255, cv2.THRESH_BINARY)

    _, binary = cv2.threshold(alpha, 127, 1, cv2.THRESH_BINARY)
    h, w = binary.shape

    # Pad by 1 pixel to handle edge paths resting on the canvas border cleanly
    padded = np.pad(binary, 1, mode='constant', constant_values=0)
    segments = {}
    
    # 1. Track Horizontal Transitions (Top and Bottom cell borders)
    for r in range(1, h + 2):
        for c in range(1, w + 1):
            top_val = padded[r-1, c]
            bot_val = padded[r, c]
            if top_val != bot_val:
                p1 = (c - 1, r - 1)
                p2 = (c, r - 1)
                if top_val < bot_val:  # Transparent -> Solid (Top edge)
                    segments[p1] = p2
                else:                  # Solid -> Transparent (Bottom edge)
                    segments[p2] = p1
                    
    # 2. Track Vertical Transitions (Left and Right cell borders)
    for r in range(1, h + 1):
        for c in range(1, w + 2):
            left_val = padded[r, c-1]
            right_val = padded[r, c]
            if left_val != right_val:
                p1 = (c - 1, r - 1)
                p2 = (c - 1, r)
                if left_val < right_val:  # Transparent -> Solid (Left edge)
                    segments[p2] = p1
                else:                     # Solid -> Transparent (Right edge)
                    segments[p1] = p2

    # 3. Chain matching segment vertices into continuous SVG path loops
    paths = []
    while segments:
        start_point = next(iter(segments))
        current_point = segments.pop(start_point)
        path = [start_point, current_point]
        
        while current_point in segments:
            next_point = segments.pop(current_point)
            path.append(next_point)
            current_point = next_point
            if current_point == start_point:
                break
        paths.append(path)
        
    # 4. Format into clean standard right-angle vector instructions
    svg_paths = []
    for path in paths:
        d_tokens = [f"M {path[0][0]} {path[0][1]}"]
        for pt in path[1:]:
            d_tokens.append(f"L {pt[0]} {pt[1]}")
        d_tokens.append("Z")
        svg_paths.append(" ".join(d_tokens))
        
    return " ".join(svg_paths)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(trace_pixel_art_exact_outline(sys.argv[1]))
    else:
        print("Error: No image path provided to the Python script.", file=sys.stderr)
