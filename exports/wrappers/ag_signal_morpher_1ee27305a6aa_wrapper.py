"""Generator-assigned wrapper hypothesis; the kernel may fit other roles."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'assays'))
from ag_signal_morpher_1ee27305a6aa_assay_platform import signal_morph

ENTRY_POINT = 'signal_morph'
run = signal_morph
