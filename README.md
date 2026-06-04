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

## Drone routes and coordinates

`config.yaml` can define multiple drones under `drones`. Route points are local simulation coordinates `[x, y, z]` in meters, centered on the loaded tile. If the city OBJ contains the Hamburg `centerE` / `centerN` metadata comments, route points can also be supplied as UTM coordinates with `{easting, northing, altitude}` or `utm: [easting, northing, altitude]`.

`sensors.yaml` uses `maxRange: 0.0` to scan to the bounds of the loaded scene instead of a fixed sensor cap.
