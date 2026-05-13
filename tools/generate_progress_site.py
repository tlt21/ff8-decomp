import html
import json
import pathlib
import re
from typing import Any

REPORT_PATH = pathlib.Path("build/report.json")
PUBLIC_DIR = pathlib.Path("public")
BADGES_DIR = PUBLIC_DIR / "badges"

REPO_PATH = ""


def slug(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def percent(value: Any) -> float:
    if value is None:
        return 0.0

    return float(value)


def format_percent(value: float) -> str:
    if value == 100:
        return "100%"

    return f"{value:.1f}%"


def color(value: float) -> str:
    if value >= 100:
        return "brightgreen"
    if value >= 80:
        return "yellowgreen"
    if value >= 50:
        return "yellow"
    if value >= 20:
        return "orange"

    return "red"


def write_json(path: pathlib.Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_badge(path: pathlib.Path, value: float) -> None:
    write_json(path, {
        "schemaVersion": 1,
        "label": "",
        "message": format_percent(value),
        "color": color(value)
    })


def measure(measures: dict[str, Any], key: str) -> float:
    return percent(measures.get(key, 0))


def main() -> None:
    report = json.loads(REPORT_PATH.read_text(encoding="utf-8"))

    BADGES_DIR.mkdir(parents=True, exist_ok=True)

    categories = report["categories"]

    for category in categories:
        category_id = slug(category["id"])
        measures = category["measures"]

        functions = measure(measures, "matched_functions_percent")
        data = measure(measures, "matched_data_percent")

        write_badge(BADGES_DIR / f"{category_id}-functions.json", functions)
        write_badge(BADGES_DIR / f"{category_id}-data.json", data)

    overall = report["measures"]
    write_badge(BADGES_DIR / "overall-functions.json", measure(overall, "matched_functions_percent"))
    write_badge(BADGES_DIR / "overall-code.json", measure(overall, "matched_code_percent"))
    write_badge(BADGES_DIR / "overall-data.json", measure(overall, "matched_data_percent"))

    write_json(PUBLIC_DIR / "report.json", report)

    rows = []

    for category in categories:
        category_id = slug(category["id"])
        name = html.escape(category["name"])
        measures = category["measures"]

        functions = format_percent(measure(measures, "matched_functions_percent"))
        data = format_percent(measure(measures, "matched_data_percent"))
        code = format_percent(measure(measures, "matched_code_percent"))

        rows.append(f"""
          <tr>
            <td>{name}</td>
            <td><img src="https://img.shields.io/endpoint?url=./badges/{category_id}-functions.json" alt="{name} functions"> {functions}</td>
            <td><img src="https://img.shields.io/endpoint?url=./badges/{category_id}-data.json" alt="{name} data"> {data}</td>
            <td>{code}</td>
          </tr>
        """)

    PUBLIC_DIR.joinpath("index.html").write_text(f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Progress Report</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {{
      font-family: system-ui, sans-serif;
      margin: 2rem;
      line-height: 1.5;
    }}

    table {{
      border-collapse: collapse;
      width: 100%;
    }}

    th, td {{
      border-bottom: 1px solid #ddd;
      padding: 0.5rem;
      text-align: left;
    }}

    th {{
      background: #f6f8fa;
    }}
  </style>
</head>
<body>
  <h1>Progress Report</h1>

  <p>
    Overall functions: {format_percent(measure(overall, "matched_functions_percent"))}<br>
    Overall code: {format_percent(measure(overall, "matched_code_percent"))}<br>
    Overall data: {format_percent(measure(overall, "matched_data_percent"))}
  </p>

  <table>
    <thead>
      <tr>
        <th>Binary</th>
        <th>Functions</th>
        <th>Data</th>
        <th>Code</th>
      </tr>
    </thead>
    <tbody>
      {''.join(rows)}
    </tbody>
  </table>
</body>
</html>
""", encoding="utf-8")


if __name__ == "__main__":
    main()