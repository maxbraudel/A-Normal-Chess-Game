from __future__ import annotations

from pathlib import Path
import subprocess
from typing import Any

from report_model import ReportPaths


def write_latex_report(
    analysis: dict[str, Any],
    insights: dict[str, Any],
    chart_artifacts: list[dict[str, Any]],
    paths: ReportPaths,
) -> None:
    content = build_latex_document(analysis, insights, chart_artifacts)
    paths.tex_path.parent.mkdir(parents=True, exist_ok=True)
    paths.tex_path.write_text(content, encoding="utf-8")


def compile_pdf_report(paths: ReportPaths) -> dict[str, Any]:
    commands = [
        [
            "pdflatex",
            "-interaction=nonstopmode",
            "-halt-on-error",
            f"-output-directory={paths.root_dir.resolve()}",
            paths.tex_path.name,
        ],
        [
            "pdflatex",
            "-interaction=nonstopmode",
            "-halt-on-error",
            f"-output-directory={paths.root_dir.resolve()}",
            paths.tex_path.name,
        ],
    ]
    outputs: list[str] = []
    success = True
    for command in commands:
        completed = subprocess.run(
            command,
            cwd=paths.root_dir,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        outputs.append(completed.stdout)
        outputs.append(completed.stderr)
        if completed.returncode != 0:
            success = False
            break

    paths.compile_log_path.write_text("\n".join(outputs), encoding="utf-8")
    return {
        "success": success and paths.pdf_path.exists(),
        "pdfPath": str(paths.pdf_path),
        "logPath": str(paths.compile_log_path),
    }


def build_latex_document(
    analysis: dict[str, Any],
    insights: dict[str, Any],
    chart_artifacts: list[dict[str, Any]],
) -> str:
    title = f"Rapport analytique - {latex_escape(analysis.get('metadata', {}).get('inputFileName', 'Partie'))}"
    sections = as_list(insights.get("sections"))
    executive_summary = as_list(insights.get("executiveSummary"))
    charts_by_section = group_charts_by_section(chart_artifacts)

    body_parts = [
        r"\documentclass[11pt,a4paper]{article}",
        r"\usepackage[utf8]{inputenc}",
        r"\usepackage[T1]{fontenc}",
        r"\usepackage[french]{babel}",
        r"\usepackage{lmodern}",
        r"\usepackage{geometry}",
        r"\usepackage{graphicx}",
        r"\usepackage{booktabs}",
        r"\usepackage{tabularx}",
        r"\usepackage{float}",
        r"\usepackage{xcolor}",
        r"\usepackage[hidelinks]{hyperref}",
        r"\geometry{margin=2cm}",
        r"\setlength{\parskip}{0.7em}",
        r"\setlength{\parindent}{0pt}",
        r"\begin{document}",
        f"\\title{{{title}}}",
        r"\author{GitHub Copilot}",
        f"\\date{{{latex_escape(insights.get('metadata', {}).get('generatedAtUtc', ''))}}}",
        r"\maketitle",
        r"\tableofcontents",
        r"\newpage",
        r"\section*{Resume executif}",
        r"\addcontentsline{toc}{section}{Resume executif}",
        r"\begin{itemize}",
    ]

    for finding in executive_summary:
        if not isinstance(finding, dict):
            continue
        body_parts.append(
            f"\\item \\textbf{{{latex_escape(finding.get('title', 'Conclusion'))}}} ({latex_escape(finding.get('confidence', 'n/a'))}) : {latex_escape(finding.get('conclusion', ''))}"
        )
    body_parts.append(r"\end{itemize}")
    body_parts.append(r"\section{Methodologie}")
    body_parts.append(
        "Ce rapport repose sur le JSON d'analyse intermediaire produit a partir du companion schema-v5. Toutes les jointures temporelles utilisent le sequence index interne, et non le numero de tour commite, car ce dernier n'est pas suffisamment unique dans cet echantillon."
    )

    for section in sections:
        if not isinstance(section, dict):
            continue
        body_parts.extend(render_section(section, charts_by_section.get(section.get("key"), [])))

    body_parts.append(r"\end{document}")
    return "\n".join(body_parts) + "\n"


def render_section(section: dict[str, Any], charts: list[dict[str, Any]]) -> list[str]:
    title = latex_escape(section.get("title", "Section"))
    parts = [f"\\section{{{title}}}", latex_escape(section.get("summary", ""))]

    metrics = as_list(section.get("headlineMetrics"))
    if metrics:
        parts.append(r"\subsection*{Indicateurs clefs}")
        parts.append(r"\begin{tabularx}{\linewidth}{lX}")
        parts.append(r"\toprule")
        parts.append(r"Indicateur & Valeur \\")
        parts.append(r"\midrule")
        for metric in metrics:
            if not isinstance(metric, dict):
                continue
            parts.append(
                f"{latex_escape(metric.get('label', ''))} & {latex_escape(metric.get('value', ''))}" + " \\\\"
            )
        parts.append(r"\bottomrule")
        parts.append(r"\end{tabularx}")

    findings = as_list(section.get("findings"))
    for finding in findings:
        if not isinstance(finding, dict):
            continue
        parts.append(
            f"\\subsection*{{{latex_escape(finding.get('title', 'Finding'))}}}"
        )
        parts.append(
            f"\\textbf{{Niveau de confiance :}} {latex_escape(finding.get('confidence', 'n/a'))}\\\\"
        )
        parts.append(latex_escape(finding.get("conclusion", "")))
        evidence = as_list(finding.get("evidence"))
        if evidence:
            parts.append(r"\begin{itemize}")
            for evidence_item in evidence:
                parts.append(f"\\item {latex_escape(evidence_item)}")
            parts.append(r"\end{itemize}")
        caveat = finding.get("caveat")
        if caveat:
            parts.append(f"\\textit{{{latex_escape(caveat)}}}")

    for chart in charts:
        parts.extend(render_chart(chart))

    tables = as_list(section.get("tables"))
    for table in tables:
        if not isinstance(table, dict):
            continue
        parts.extend(render_table(table))

    return parts


def render_chart(chart: dict[str, Any]) -> list[str]:
    return [
        r"\begin{figure}[H]",
        r"\centering",
        f"\\includegraphics[width=0.98\\linewidth]{{\\detokenize{{{chart.get('relativePath', '')}}}}}",
        f"\\caption{{{latex_escape(chart.get('caption', ''))}}}",
        r"\end{figure}",
    ]


def render_table(table: dict[str, Any]) -> list[str]:
    columns = as_list(table.get("columns"))
    rows = as_list(table.get("rows"))
    if not columns:
        return []
    column_spec = "|".join(["X"] * len(columns))
    parts = [
        f"\\subsection*{{{latex_escape(table.get('title', 'Tableau'))}}}",
        f"\\begin{{tabularx}}{{\\linewidth}}{{{column_spec}}}",
        r"\toprule",
        " & ".join(latex_escape(column.get("label", "")) for column in columns) + r" \\",
        r"\midrule",
    ]
    for row in rows:
        if not isinstance(row, dict):
            continue
        parts.append(
            " & ".join(
                latex_escape(stringify_table_value(row.get(column.get("key"))))
                for column in columns
            )
            + " \\\\"
        )
    parts.extend([r"\bottomrule", r"\end{tabularx}"])
    return parts


def group_charts_by_section(chart_artifacts: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for chart in chart_artifacts:
        if not isinstance(chart, dict):
            continue
        grouped.setdefault(str(chart.get("sectionKey", "overview")), []).append(chart)
    return grouped


def stringify_table_value(value: Any) -> str:
    if value is None:
        return "n/a"
    return str(value)


def latex_escape(value: Any) -> str:
    replacements = {
        "\\": r"\textbackslash{}",
        "&": r"\&",
        "%": r"\%",
        "$": r"\$",
        "#": r"\#",
        "_": r"\_",
        "{": r"\{",
        "}": r"\}",
        "~": r"\textasciitilde{}",
        "^": r"\textasciicircum{}",
    }
    return "".join(replacements.get(character, character) for character in str(value))


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []