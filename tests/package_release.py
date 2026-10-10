"""Entry point for the current verified CalcAero release."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("package_v13.py")),run_name="__main__")
