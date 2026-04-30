from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import json
from pathlib import Path
from statistics import mean
from typing import Any


REPORT_VERSION = "0.1.0"


@dataclass(slots=True)
class ReportPaths:
    root_dir: Path
    charts_dir: Path
    analysis_json_path: Path
    insights_json_path: Path
    tex_path: Path
    pdf_path: Path
    compile_log_path: Path


def default_output_root() -> Path:
    return Path(__file__).resolve().parents[1] / "output"


def derive_report_name(input_path: Path) -> str:
    name = input_path.stem
    if name.endswith(".analysis"):
        name = name[: -len(".analysis")]
    return name


def resolve_report_paths(input_path: Path, requested_output_dir: str | None) -> ReportPaths:
    report_name = derive_report_name(input_path)
    if requested_output_dir:
        root_dir = Path(requested_output_dir)
    else:
        root_dir = default_output_root() / report_name

    return ReportPaths(
        root_dir=root_dir,
        charts_dir=root_dir / "charts",
        analysis_json_path=root_dir / f"{report_name}.analysis.json",
        insights_json_path=root_dir / f"{report_name}.report.json",
        tex_path=root_dir / f"{report_name}.tex",
        pdf_path=root_dir / f"{report_name}.pdf",
        compile_log_path=root_dir / f"{report_name}.pdflatex.log.txt",
    )


def load_json_file(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return payload


def write_json_file(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2)
        handle.write("\n")


def utc_timestamp() -> str:
    return datetime.now(timezone.utc).isoformat()


def safe_mean(values: list[int | float]) -> float | None:
    if not values:
        return None
    return float(mean(values))


def safe_max(values: list[int | float], default: int | float = 0) -> int | float:
    if not values:
        return default
    return max(values)


def safe_min(values: list[int | float], default: int | float = 0) -> int | float:
    if not values:
        return default
    return min(values)


def safe_div(numerator: int | float, denominator: int | float) -> float | None:
    if not denominator:
        return None
    return float(numerator) / float(denominator)


def round_or_none(value: float | None, digits: int = 2) -> float | None:
    if value is None:
        return None
    return round(value, digits)


def format_number(value: Any, digits: int = 2) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, bool):
        return "oui" if value else "non"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        if value.is_integer():
            return str(int(value))
        return f"{value:.{digits}f}"
    return str(value)


def format_percent(ratio: float | None, digits: int = 1) -> str:
    if ratio is None:
        return "n/a"
    return f"{ratio * 100:.{digits}f}%"


def rows_to_count_map(rows: list[dict[str, Any]]) -> dict[str, int]:
    result: dict[str, int] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        key = None
        for candidate in ("typeKey", "sourceKey", "key", "value", "kingdomKey"):
            value = row.get(candidate)
            if value is not None:
                key = str(value)
                break
        if key is None:
            continue
        count = row.get("count")
        if isinstance(count, (int, float)):
            result[key] = int(count)
    return result


def confidence_label(sample_size: int) -> str:
    if sample_size >= 30:
        return "forte"
    if sample_size >= 8:
        return "moderee"
    return "exploratoire"


def build_table(title: str, columns: list[tuple[str, str]], rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "title": title,
        "columns": [{"key": key, "label": label} for key, label in columns],
        "rows": rows,
    }


def headline_metric(label: str, value: Any) -> dict[str, str]:
    return {"label": label, "value": format_number(value)}


def percent_metric(label: str, ratio: float | None) -> dict[str, str]:
    return {"label": label, "value": format_percent(ratio)}