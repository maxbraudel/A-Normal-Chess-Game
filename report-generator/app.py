from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
from typing import Any

from charts import render_report_charts
from insights import build_report_insights
from latex_renderer import compile_pdf_report, write_latex_report
from report_model import REPORT_VERSION, load_json_file, resolve_report_paths, write_json_file


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a themed French report from one analysis JSON or one companion JSON."
        )
    )
    parser.add_argument("input", help="Path to the analysis JSON or companion JSON file")
    parser.add_argument(
        "-o",
        "--output-dir",
        help="Output directory for charts, report JSON, TeX and PDF",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Allow writing into an existing non-empty output directory",
    )
    parser.add_argument(
        "--skip-pdf",
        action="store_true",
        help="Skip pdflatex compilation and keep only charts plus the TeX source",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.is_file():
        parser.error(f"Input file does not exist: {input_path}")

    paths = resolve_report_paths(input_path, args.output_dir)
    if paths.root_dir.exists() and any(paths.root_dir.iterdir()) and not args.overwrite:
        parser.error(
            f"Output directory already exists and is not empty: {paths.root_dir}. Use --overwrite to replace files inside it."
        )

    paths.root_dir.mkdir(parents=True, exist_ok=True)
    paths.charts_dir.mkdir(parents=True, exist_ok=True)

    analysis = load_or_generate_analysis(input_path, paths)
    insights = build_report_insights(analysis, analysis_source_name=paths.analysis_json_path.name)
    chart_artifacts = render_report_charts(analysis, insights, paths.charts_dir)
    write_json_file(paths.insights_json_path, build_report_payload(analysis, insights, chart_artifacts))
    write_latex_report(analysis, insights, chart_artifacts, paths)

    pdf_result = {"success": False, "pdfPath": None, "logPath": None}
    if not args.skip_pdf:
        pdf_result = compile_pdf_report(paths)

    print(paths.insights_json_path)
    print(paths.tex_path)
    if pdf_result.get("success"):
        print(paths.pdf_path)
    return 0


def load_or_generate_analysis(input_path: Path, paths: Any) -> dict[str, Any]:
    payload = load_json_file(input_path)
    if is_analysis_payload(payload):
        write_json_file(paths.analysis_json_path, payload)
        return payload

    analysis = extract_analysis_from_companion(input_path)
    write_json_file(paths.analysis_json_path, analysis)
    return analysis


def is_analysis_payload(payload: dict[str, Any]) -> bool:
    metadata = payload.get("metadata")
    return isinstance(metadata, dict) and "analysisVersion" in metadata


def extract_analysis_from_companion(input_path: Path) -> dict[str, Any]:
    workspace_root = Path(__file__).resolve().parents[1]
    extractor_path = workspace_root / "analysis-generator" / "extractor.py"
    if not extractor_path.is_file():
        raise FileNotFoundError(f"Extractor module not found: {extractor_path}")

    sys.path.insert(0, str(extractor_path.parent))
    try:
        spec = importlib.util.spec_from_file_location(
            "analysis_generator_extractor", extractor_path
        )
        if spec is None or spec.loader is None:
            raise RuntimeError(f"Unable to load extractor module: {extractor_path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        extract_analysis = getattr(module, "extract_analysis")
        analysis = extract_analysis(input_path)
        if not isinstance(analysis, dict):
            raise ValueError("Extractor did not return a JSON object")
        return analysis
    finally:
        if sys.path and sys.path[0] == str(extractor_path.parent):
            sys.path.pop(0)


def build_report_payload(
    analysis: dict[str, Any],
    insights: dict[str, Any],
    chart_artifacts: list[dict[str, Any]],
) -> dict[str, Any]:
    return {
        "metadata": {
            "reportVersion": REPORT_VERSION,
            "analysisVersion": analysis.get("metadata", {}).get("analysisVersion"),
            "inputFileName": analysis.get("metadata", {}).get("inputFileName"),
        },
        "insights": insights,
        "charts": chart_artifacts,
    }


if __name__ == "__main__":
    raise SystemExit(main())