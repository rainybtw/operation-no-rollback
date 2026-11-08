"""No Rollback: builds the world map images used by the game.

Outputs (in ./assets):
  world_base_cool.png, world_base_warm.png   the visible map
  continent_mask.png                          continent IDs in the red channel (0 = ocean, 1..6)

All images share one projection and size, so a pixel in the mask is the same place in the map.
Data: Natural Earth (public domain) for land, roads, railroads and cities; OpenFlights (ODbL) for
flight routes. The submarine cables are hand-drawn ILLUSTRATIVE corridors, not survey data.

Usage: python make_map.py [mask|cool|warm|all]
Needs: pip install matplotlib numpy shapely pillow
"""
import csv
import json
import math
import os
import sys
import urllib.request

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.collections import LineCollection, PatchCollection
from matplotlib.patches import Polygon as MplPolygon
from PIL import Image, ImageDraw
from shapely.geometry import box, shape

# ---------------------------------------------------------------- settings
MAP_WIDTH = 8192
LON_MIN, LON_MAX = -180.0, 180.0
LAT_MIN, LAT_MAX = -58.0, 84.0
MAP_HEIGHT = round(MAP_WIDTH * (LAT_MAX - LAT_MIN) / (LON_MAX - LON_MIN))   # 3232
DPI = 400
OUTPUT_DIR = "assets"
URALS_LONGITUDE = 60.0          # Natural Earth tags all of Russia as Europe, so we split it here
MIN_FLIGHT_KM = 2500            # short hops would only add clutter

CONTINENT_IDS = {"North America": 1, "South America": 2, "Europe": 3,
                 "Africa": 4, "Asia": 5, "Oceania": 6}

NATURAL_EARTH = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson"
OPENFLIGHTS = "https://raw.githubusercontent.com/jpatokal/openflights/master/data"

# Orange-red is reserved for the "hacked" color in the game, so the map itself never uses it.
PALETTES = {
    "cool": dict(bg="#050913", land="#0a1322", border="#22344f", rail="#2a4468", road="#2f4d78",
                 highway="#4a77b0", flight="#6f9be0", cable="#3fe0ec", city="#e6eefb", halo="#6fa8ff"),
    "warm": dict(bg="#1c0d08", land="#2a130b", border="#4a3826", rail="#5a4630", road="#6b5638",
                 highway="#a07a48", flight="#d09a5a", cable="#ffd070", city="#ffe3a8", halo="#ffa65a"),
}

# Illustrative submarine cable corridors as (longitude, latitude) waypoints through open water.
CABLE_ROUTES = [
    [(-74.0, 40.0), (-60, 40.5), (-40, 44.5), (-20, 48.5), (-5.5, 50.1)],
    [(-70.9, 42.2), (-55, 45), (-30, 51.5), (-10, 53.5)],
    [(-75.9, 36.8), (-60, 38), (-35, 41), (-15, 43), (-3.0, 43.7)],
    [(-79.5, 26.5), (-72, 26), (-62, 22), (-52, 12), (-44, 3), (-40, -1), (-38.5, -3.7)],
    [(-38.5, -3.7), (-20, -2), (0, 0), (3.4, 6.2)],
    [(-46.3, -24.5), (-25, -30), (0, -34), (18.2, -34.2)],
    [(-9.5, 38.5), (-12, 30), (-18, 22), (-18, 14.7), (-15, 6), (-5, 3), (3.4, 6.2),
     (6, -3), (10, -8), (12, -17), (12, -26), (15, -32), (18.2, -34.2)],
    [(5.4, 43.0), (10, 38.5), (20, 35.2), (29.8, 31.4)],
    [(32.5, 29.9), (34, 27.5), (36, 23), (38.5, 19), (41, 15.5), (43.3, 12.6), (48, 12.2),
     (55, 14.5), (65, 17), (72.5, 18.8)],
    [(72.5, 18.8), (62, 21), (58.5, 24.5), (56.4, 25.3)],
    [(72.5, 18.8), (73, 10), (77, 5.5), (85, 5), (92, 6), (96.5, 6.2), (98.7, 5.3), (100.2, 4.0),
     (101.0, 3.0), (102.3, 1.7), (103.5, 1.1)],
    [(103.5, 1.1), (108, 5), (110.5, 10), (112.5, 16), (114.2, 21.8)],
    [(114.2, 21.8), (119.0, 23.0), (123, 26.2), (128, 30), (133, 32.5), (138, 33), (140.0, 34.6)],
    [(140.3, 34.7), (155, 37), (172, 40), (-170, 42), (-150, 43), (-135, 41), (-125.0, 40.2)],
    [(140.3, 34.7), (142, 25), (144.8, 13.6), (170, 17), (-175, 19), (-157.9, 21.3)],
    [(-157.9, 21.3), (-140, 28), (-123.2, 37.0)],
    [(151.3, -33.9), (162, -36.5), (174.5, -36.3), (-176, -22), (-172, -10), (-160, 5), (-157.9, 21.3)],
    [(-118.5, 33.8), (-118, 30), (-110, 20), (-95, 12), (-85, 7), (-81, 3), (-81, -4), (-78.5, -12.5),
     (-73, -24), (-73, -33)],
    [(18.2, -34.2), (32, -35.5), (40, -26), (42, -15), (41.5, -8), (40.5, -4.5)],
    [(40.5, -4.5), (50, -2), (55, 8), (58, 17), (57, 24.5)],
]


# ---------------------------------------------------------------- data loading
def download_if_missing(url, path):
    if not os.path.exists(path):
        print("downloading", os.path.basename(path))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        urllib.request.urlretrieve(url, path)
    return path


def load_features(name):
    path = download_if_missing(f"{NATURAL_EARTH}/{name}.geojson", f"data/{name}.geojson")
    with open(path) as file:
        return json.load(file)["features"]


def load_flight_routes():
    """Returns long-haul routes as ((lon1, lat1), (lon2, lat2)) pairs, one per airport pair."""
    airports_file = download_if_missing(f"{OPENFLIGHTS}/airports.dat", "data/airports.dat")
    routes_file = download_if_missing(f"{OPENFLIGHTS}/routes.dat", "data/routes.dat")
    positions = {}
    with open(airports_file, encoding="utf-8") as file:
        for row in csv.reader(file):
            positions[row[0]] = (float(row[7]), float(row[6]))   # (lon, lat)
    pairs = set()
    with open(routes_file, encoding="utf-8") as file:
        for row in csv.reader(file):
            source, destination = row[3], row[5]
            if source in positions and destination in positions:
                pairs.add(tuple(sorted((source, destination))))
    return [(positions[a], positions[b]) for a, b in pairs]


# ---------------------------------------------------------------- geometry helpers
def great_circle(start, end, samples=40):
    """Points along the shortest path between two (lon, lat) points, plus its length in km."""
    lon1, lat1, lon2, lat2 = map(math.radians, (*start, *end))
    a = np.array([math.cos(lat1) * math.cos(lon1), math.cos(lat1) * math.sin(lon1), math.sin(lat1)])
    b = np.array([math.cos(lat2) * math.cos(lon2), math.cos(lat2) * math.sin(lon2), math.sin(lat2)])
    angle = math.acos(max(-1.0, min(1.0, float(a @ b))))
    if angle < 1e-6:
        return np.array([start, end]), 0.0
    t = np.linspace(0, 1, samples)[:, None]
    points = (np.sin((1 - t) * angle) * a + np.sin(t * angle) * b) / math.sin(angle)
    lons = np.degrees(np.arctan2(points[:, 1], points[:, 0]))
    lats = np.degrees(np.arcsin(np.clip(points[:, 2], -1, 1)))
    return np.column_stack([lons, lats]), angle * 6371.0


def split_at_dateline(points):
    """Cuts a path wherever it jumps across the +-180 meridian, so no line spans the whole map."""
    jumps = np.where(np.abs(np.diff(points[:, 0])) > 180)[0] + 1
    return [piece for piece in np.split(points, jumps) if len(piece) > 1]


def unwrap_longitudes(waypoints):
    result = [list(waypoints[0])]
    for lon, lat in waypoints[1:]:
        previous = result[-1][0]
        while lon - previous > 180:
            lon -= 360
        while lon - previous < -180:
            lon += 360
        result.append([lon, lat])
    return np.array(result, dtype=float)


def smooth_curve(points, samples=24):
    """Catmull-Rom spline through the waypoints."""
    padded = np.vstack([points[0], points, points[-1]])
    curve = []
    for i in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[i - 1], padded[i], padded[i + 1], padded[i + 2]
        for t in np.linspace(0, 1, samples, endpoint=False):
            curve.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t ** 2
                                + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    curve.append(padded[-2])
    return np.array(curve)


def cable_paths():
    paths = []
    for waypoints in CABLE_ROUTES:
        curve = smooth_curve(unwrap_longitudes(waypoints))
        curve[:, 0] = (curve[:, 0] + 180) % 360 - 180
        paths += split_at_dateline(curve)
    return paths


def flight_paths(routes):
    paths = []
    for start, end in routes:
        points, kilometers = great_circle(start, end)
        if kilometers >= MIN_FLIGHT_KM:
            paths += split_at_dateline(points)
    return paths


def line_segments(features, keep=lambda properties: True):
    segments = []
    for feature in features:
        geometry = feature["geometry"]
        if geometry is None or not keep(feature["properties"]):
            continue
        if geometry["type"] == "LineString":
            parts = [geometry["coordinates"]]
        elif geometry["type"] == "MultiLineString":
            parts = geometry["coordinates"]
        else:
            continue
        segments += [np.asarray(part)[:, :2] for part in parts if len(part) > 1]
    return segments


# ---------------------------------------------------------------- continent mask
def to_pixels(lon, lat):
    x = (lon - LON_MIN) / (LON_MAX - LON_MIN) * MAP_WIDTH
    y = (LAT_MAX - lat) / (LAT_MAX - LAT_MIN) * MAP_HEIGHT
    return x, y


def continent_for_overseas_piece(part):
    """Pieces of European countries far from Europe (French Guiana, Reunion...) get their real continent."""
    lon, lat = part.centroid.x, part.centroid.y
    if LON_MIN / 6 <= lon <= URALS_LONGITUDE and lat >= 30:
        return CONTINENT_IDS["Europe"]
    if lon < -100 and lat < 0:
        return CONTINENT_IDS["Oceania"]
    if lon < -25:
        return CONTINENT_IDS["North America"] if lat > 12 else CONTINENT_IDS["South America"]
    if lon <= URALS_LONGITUDE:
        return CONTINENT_IDS["Africa"]
    if lon > 100 and lat < 0:
        return CONTINENT_IDS["Oceania"]
    return None


def build_mask(countries):
    mask = Image.new("RGB", (MAP_WIDTH, MAP_HEIGHT), (0, 0, 0))
    pen = ImageDraw.Draw(mask)

    def fill(shape_to_fill, continent_id):
        if shape_to_fill.is_empty:
            return
        if shape_to_fill.geom_type in ("MultiPolygon", "GeometryCollection"):
            pieces = list(shape_to_fill.geoms)
        else:
            pieces = [shape_to_fill]
        for piece in pieces:
            if piece.geom_type == "Polygon":
                pen.polygon([to_pixels(x, y) for x, y in piece.exterior.coords], fill=(continent_id, 0, 0))

    for country in countries:
        properties = country["properties"]
        continent_id = CONTINENT_IDS.get(properties.get("CONTINENT"))
        if continent_id is None:
            continue
        geometry = shape(country["geometry"])
        parts = list(geometry.geoms) if geometry.geom_type == "MultiPolygon" else [geometry]
        for part in parts:
            if properties.get("ADMIN") == "Russia":
                far_east = part.centroid.x < -100 or (part.centroid.x > 100 and part.bounds[0] > URALS_LONGITUDE)
                if far_east:
                    fill(part, CONTINENT_IDS["Asia"])
                else:
                    fill(part.intersection(box(URALS_LONGITUDE, -90, 180, 90)), CONTINENT_IDS["Asia"])
                    fill(part.intersection(box(-180, -90, URALS_LONGITUDE, 90)), CONTINENT_IDS["Europe"])
            elif continent_id == CONTINENT_IDS["Europe"]:
                overseas_id = continent_for_overseas_piece(part)
                if overseas_id:
                    fill(part, overseas_id)
            else:
                fill(part, continent_id)
    mask.save(f"{OUTPUT_DIR}/continent_mask.png")
    print("saved continent_mask.png", mask.size)


# ---------------------------------------------------------------- visible map
def land_polygons(countries):
    polygons = []
    for country in countries:
        if country["properties"].get("CONTINENT") == "Antarctica":
            continue
        geometry = country["geometry"]
        if geometry["type"] == "Polygon":
            rings = [geometry["coordinates"][0]]
        elif geometry["type"] == "MultiPolygon":
            rings = [polygon[0] for polygon in geometry["coordinates"]]
        else:
            continue
        polygons += [MplPolygon(np.asarray(ring)[:, :2], closed=True) for ring in rings]
    return polygons


def render_base(name, countries, layers):
    palette = PALETTES[name]
    figure = plt.figure(figsize=(MAP_WIDTH / DPI, MAP_HEIGHT / DPI), dpi=DPI, facecolor=palette["bg"])
    axes = figure.add_axes([0, 0, 1, 1])
    axes.set_facecolor(palette["bg"])
    axes.set_xlim(LON_MIN, LON_MAX)
    axes.set_ylim(LAT_MIN, LAT_MAX)
    axes.set_aspect("auto")
    axes.axis("off")

    axes.add_collection(PatchCollection(layers["land"], facecolor=palette["land"],
                                        edgecolor=palette["border"], linewidth=0.3))
    axes.add_collection(LineCollection(layers["rails"], colors=palette["rail"], linewidths=0.12, alpha=0.35))
    axes.add_collection(LineCollection(layers["roads"], colors=palette["road"], linewidths=0.14, alpha=0.6))
    axes.add_collection(LineCollection(layers["flights"], colors=palette["flight"], linewidths=0.16, alpha=0.20))
    axes.add_collection(LineCollection(layers["highways"], colors=palette["highway"], linewidths=0.3, alpha=0.8))
    axes.add_collection(LineCollection(layers["cables"], colors=palette["cable"], linewidths=2.2, alpha=0.10))
    axes.add_collection(LineCollection(layers["cables"], colors=palette["cable"], linewidths=0.55, alpha=0.95))

    cities = layers["cities"]
    size = (np.log10(np.clip(cities[:, 2], 1e4, 3e7)) - 3.9) ** 2.2
    axes.scatter(cities[:, 0], cities[:, 1], s=size * 9, color=palette["halo"], alpha=0.10, linewidths=0, zorder=5)
    axes.scatter(cities[:, 0], cities[:, 1], s=size * 1.6, color=palette["city"], alpha=0.85, linewidths=0, zorder=6)

    path = f"{OUTPUT_DIR}/world_base_{name}.png"
    figure.savefig(path, facecolor=palette["bg"])
    plt.close(figure)
    image = Image.open(path)
    if image.size != (MAP_WIDTH, MAP_HEIGHT):      # guarantee exact alignment with the mask
        image.convert("RGB").resize((MAP_WIDTH, MAP_HEIGHT), Image.LANCZOS).save(path)
    print("saved", path, Image.open(path).size)


def build_layers(countries):
    roads = load_features("ne_10m_roads")
    cities = load_features("ne_10m_populated_places_simple")
    return {
        "land": land_polygons(countries),
        "rails": line_segments(load_features("ne_10m_railroads")),
        "roads": line_segments(roads, lambda p: p.get("type") != "Major Highway"),
        "highways": line_segments(roads, lambda p: p.get("type") == "Major Highway"),
        "flights": flight_paths(load_flight_routes()),
        "cables": cable_paths(),
        "cities": np.array([[c["geometry"]["coordinates"][0], c["geometry"]["coordinates"][1],
                             c["properties"].get("pop_max") or 0] for c in cities]),
    }


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    countries = load_features("ne_10m_admin_0_countries")
    if target in ("mask", "all"):
        build_mask(countries)
    names = [n for n in PALETTES if target in (n, "all")]
    if names:
        layers = build_layers(countries)
        print("flight arcs:", len(layers["flights"]), "cable paths:", len(layers["cables"]))
        for name in names:
            render_base(name, countries, layers)


if __name__ == "__main__":
    main()
