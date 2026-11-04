# eyes format



```json
{
  "image": {
    "width": 4032,
    "height": 3024,
    "format": "heic"
  },
  "exif": {
    "parsed": true,
    "ByteAlign": 1,
    "Make": "Apple",
    "Model": "iPhone 15 Pro",
    "Software": "18.5",
    "ImageDescription": "",
    "Orientation": 6,
    "BitsPerSample": 8,
    "DateTime": "2026:07:19 14:30:00",
    "DateTimeOriginal": "2026:07:19 14:30:00",
    "DateTimeDigitized": "2026:07:19 14:30:00",
    "SubSecTimeOriginal": "123",
    "Copyright": "",
    "ExposureTime": 0.008333,
    "FNumber": 1.78,
    "ExposureProgram": 2,
    "ISOSpeedRatings": 64,
    "ShutterSpeedValue": 6.91,
    "ExposureBiasValue": 0.0,
    "SubjectDistance": 0.0,
    "FocalLength": 6.86,
    "FocalLengthIn35mm": 24,
    "Flash": 1,
    "FlashReturnedLight": 0,
    "FlashMode": 0,
    "MeteringMode": 5,
    "ImageWidth": 4032,
    "ImageHeight": 3024,
    "GPS": {
      "Latitude": 37.774929,
      "Longitude": -122.419418,
      "Altitude": 10.5,
      "AltitudeRef": 0,
      "DOP": 5.0,
      "LatDegrees": 37.0,
      "LatMinutes": 46.0,
      "LatSeconds": 29.74,
      "LatDirection": "N",
      "LonDegrees": 122.0,
      "LonMinutes": 25.0,
      "LonSeconds": 9.9,
      "LonDirection": "W"
    },
    "Lens": {
      "Make": "Apple",
      "Model": "iPhone 15 Pro back triple camera 6.86mm f/1.78",
      "FStopMin": 1.78,
      "FStopMax": 1.78,
      "FocalLengthMin": 6.86,
      "FocalLengthMax": 6.86,
      "FocalPlaneXResolution": 4032.0,
      "FocalPlaneYResolution": 3024.0,
      "FocalPlaneResolutionUnit": 2
    }
  },
  "objects": [
    {
      "label": "person",
      "confidence": 0.91,
      "bounds": { "x": 0.18, "y": 0.08, "w": 0.34, "h": 0.88 }
    },
    {
      "label": "cup",
      "confidence": 0.73,
      "bounds": { "x": 0.62, "y": 0.71, "w": 0.09, "h": 0.12 }
    },
    {
      "label": "chair",
      "confidence": 0.58,
      "bounds": { "x": 0.05, "y": 0.45, "w": 0.28, "h": 0.42 }
    }
  ],
  "ocr": [
    {
      "text": "OPEN 9:41 AM",
      "confidence": 0.8,
      "bounds": { "x": 0.10, "y": 0.02, "w": 0.50, "h": 0.06 }
    }
  ],
  "codes": [
    {
      "text": "https://example.com/menu",
      "format": "QRCode",
      "bounds": { "x": 0.71, "y": 0.52, "w": 0.18, "h": 0.18 },
      "inverted": false
    },
    {
      "text": "012345678905",
      "format": "EAN-13",
      "bounds": { "x": 0.08, "y": 0.55, "w": 0.35, "h": 0.06 },
      "inverted": false
    }
  ],
  "faces": [
    {
      "confidence": 0.98,
      "bounds": { "x": 0.28, "y": 0.14, "w": 0.14, "h": 0.19 },
      "id": "9f2c1a0b7e6d5c4a3b2f1e0d9c8b7a69584736251403f2e1d0c9b8a796857463",
      "personId": "1a2b3c4d5e6f7081928374655647382910abcdef0123456789abcdef01234567"
    },
    {
      "confidence": 0.87,
      "bounds": { "x": 0.55, "y": 0.22, "w": 0.10, "h": 0.13 },
      "id": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "personId": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    }
  ],
  "description": {
    "tags": [
      { "label": "espresso", "score": 8.42 },
      { "label": "coffee mug", "score": 6.17 },
      { "label": "restaurant", "score": 5.03 },
      { "label": "table", "score": 4.88 },
      { "label": "indoor", "score": 3.91 },
      { "label": "cup", "score": 3.44 },
      { "label": "tray", "score": 2.76 },
      { "label": "saucer", "score": 2.11 },
      { "label": "dining table", "score": 1.85 },
      { "label": "food", "score": 1.52 }
    ],
    "caption": "Two people sitting at a table in a cafe with a coffee cup"
  },
  "extras": {
    "phash": "a3f2c1d0e9b8a7f6",
    "blurScore": 8.742,
    "dominantColors": [
      { "hex": "#c0a080", "weight": 2841 },
      { "hex": "#604020", "weight": 1923 },
      { "hex": "#e0c0a0", "weight": 1456 },
      { "hex": "#202020", "weight": 987 },
      { "hex": "#808080", "weight": 654 },
      { "hex": "#a06040", "weight": 432 },
      { "hex": "#e0e0e0", "weight": 301 },
      { "hex": "#404040", "weight": 198 }
    ]
  }
}
```

`id` and `personId` are present when `analyze` is given a data directory. Each new face is stored once at `dataDir/faces/<id>.bin`: 512 little-endian float32 values, L2-normalized. `<id>` is the SHA-256 of those bytes. The embedding file is not rewritten. `dataDir/faces/<id>.person` holds the `personId` chosen for that face.

`personId` is the `id` of the first face in that cluster. A new face joins a stored face when cosine similarity is at least 0.45, comparing against every face already in the gallery, and keeps that person's id. The data directory is the module directory inside the synced `.space` folder, so another device analyzing a later photo uses the same gallery.

Detections without a data directory omit `id` and `personId`.

Module version `2.0.0` is the generation that fills `personId`. The JSON shape is unchanged from `1.0.0`. A `1.0.0` sidecar stays readable.

`<id>.person` is the live assignment: one line, the 64-character id of a face in the cluster. A face with no match uses its own id. Analysis writes `<id>.person` only when that file is absent, so a later rewrite is kept. A line that is not 64 characters is ignored and the face is its own person. The JSON sidecar copies `id` and `personId` at analysis time and is not rewritten when `<id>.person` changes. After that change, `<id>.person` is the assignment. A running analysis keeps the assignment it already loaded; the next start reads the file again.

A display name is not part of this JSON. It belongs in a separate synced record keyed by person id, for example `people/<personId>` beside `faces/`.