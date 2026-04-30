# Analysis Reporting

Ce dossier regroupe les deux outils Python de la pipeline d'analyse et de rapport.

## Structure

- `analysis-generator/` : transforme un companion JSON en fichier d'analyse `.analysis.json`
- `report-generator/` : transforme un companion JSON ou un `.analysis.json` en rapport JSON + LaTeX + PDF
- `output/` : dossier de sortie par defaut pour les fichiers generes

## Prerequis

- Activer le venv du projet
- Avoir `pdflatex` disponible si vous voulez generer le PDF final
- Avoir `matplotlib` installe dans le venv pour generer les graphiques du rapport

## Interpreteur Python

Toutes les commandes ci-dessous utilisent explicitement le Python du venv du projet :

```powershell
& ".\.venv\Scripts\python.exe"
```

Si `matplotlib` n'est pas encore installe dans ce venv, faites d'abord :

```powershell
& ".\.venv\Scripts\python.exe" -m pip install matplotlib
```

## Commandes

Depuis la racine du projet.

### 1. Generer les stats

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\analysis-generator\app.py" ".\build\Data\KAZIMIRIUM 1.json" --pretty --overwrite
```

Sortie par defaut :

```text
.\analysis-reporting\output\KAZIMIRIUM 1.analysis.json
```

### 2. Generer le rapport LaTeX + PDF a partir du fichier d'analyse

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\report-generator\app.py" ".\analysis-reporting\output\KAZIMIRIUM 1.analysis.json" --overwrite
```

Sorties par defaut :

```text
.\analysis-reporting\output\KAZIMIRIUM 1\KAZIMIRIUM 1.analysis.json
.\analysis-reporting\output\KAZIMIRIUM 1\KAZIMIRIUM 1.report.json
.\analysis-reporting\output\KAZIMIRIUM 1\KAZIMIRIUM 1.tex
.\analysis-reporting\output\KAZIMIRIUM 1\KAZIMIRIUM 1.pdf
```

### 3. Generer directement le rapport depuis le companion JSON

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\report-generator\app.py" ".\build\Data\KAZIMIRIUM 1.json" --overwrite
```

Dans ce cas, le report generator regenere d'abord le `.analysis.json` dans `analysis-reporting/output/`, puis construit le rapport complet dans le sous-dossier correspondant.

### 4. Generer seulement le LaTeX sans compiler le PDF

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\report-generator\app.py" ".\analysis-reporting\output\KAZIMIRIUM 1.analysis.json" --overwrite --skip-pdf
```

### 5. Forcer un dossier de sortie personnalise

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\report-generator\app.py" ".\analysis-reporting\output\KAZIMIRIUM 1.analysis.json" --overwrite --output-dir ".\analysis-reporting\output\mon-rapport"
```

## Exemple de sequence complete

```powershell
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\analysis-generator\app.py" ".\build\Data\KAZIMIRIUM 1.json" --pretty --overwrite
& ".\.venv\Scripts\python.exe" ".\analysis-reporting\report-generator\app.py" ".\analysis-reporting\output\KAZIMIRIUM 1.analysis.json" --overwrite
```