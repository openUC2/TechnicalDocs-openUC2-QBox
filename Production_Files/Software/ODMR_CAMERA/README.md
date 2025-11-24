
# ✔ Example Serial Commands

### 1) Set exposure and gain

```
{"cmd":"set","exposure":500,"gain":0}
```

### 2) Get mean only

```
{"cmd":"get_frame"}
```

### Response

```json
{"mean":72.4}
```

### 3) Get projections too

```
{"cmd":"get_frame","projections":true}
```

### Response

```json
{
  "mean": 71.8,
  "projX": [ ... w values ... ],
  "projY": [ ... h values ... ]
}
```

---

# ✔ Notes / Customization

* Currently grayscale for simplicity (faster math, less RAM).
* You can swap to RGB565/YCbCr if you need color projections.
* If you want **continuous streaming**, we can add a `"start_stream"` mode that pushes stats every frame without being polled.
* If you want **binary instead of JSON**, I can produce a compact protocol.

---

If you want, I can also provide:

✅ A matching **Python serial client**
✅ A FastAPI API wrapper
✅ A ROS2 interface
✅ A version where the ESP32 also performs **thresholding / histogram / centroid detection**

Just say what you want.
