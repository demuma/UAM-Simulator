# Getting started

## Activate virtual environment:
````
. .venv/bin/activate
```

## Download Hamburg 3D CityGMl dataset:
```
cd map
wget https://daten-hamburg.de/opendata/3d_stadtmodell_lod3/LoD3-HH_Area1_2023_12_14.zip

```
## Extract the tile, upscale the textures and convert to .obj:
```
python upscale_textures.py
python extract_clip.py
```

## Build with:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel
```

## Run with:
```
./build/UAM-Simulator
```
