from __future__ import annotations

import argparse
import json
from pathlib import Path

from extractor import extract_analysis


def default_output_dir() -> Path:
    return Path(__file__).resolve().parents[1] / "output"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate a structured analysis JSON from one companion file."
    )
    parser.add_argument("input", help="Path to the companion JSON file")
    parser.add_argument(
        "-o",
        "--output",
        help="Output path for the generated analysis JSON",
    )
    parser.add_argument(
        "--pretty",
        action="store_true",
        help="Write the JSON with indentation",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Allow overwriting the output file if it already exists",
    )
    return parser


def resolve_output_path(input_path: Path, requested_output: str | None) -> Path:
    if requested_output:
        return Path(requested_output)

    return default_output_dir() / f"{input_path.stem}.analysis.json"


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = resolve_output_path(input_path, args.output)

    if not input_path.is_file():
        parser.error(f"Input file does not exist: {input_path}")

    if output_path.exists() and not args.overwrite:
        parser.error(
            f"Output file already exists: {output_path}. Use --overwrite to replace it."
        )

    analysis = extract_analysis(input_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(
            analysis,
            handle,
            ensure_ascii=False,
            indent=2 if args.pretty else None,
        )
        handle.write("\n")

    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())