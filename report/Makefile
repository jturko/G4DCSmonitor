# Makefile for the LaTeX report (matrix_report.tex -> matrix_report.pdf).
#
#   make -C report              build the PDF (default target)
#   make -C report pdf          same thing
#   make -C report figures      regenerate every figure first, then the PDF is
#                               built on the next invocation (needs the data;
#                               delegates to macros/Makefile.report)
#   make -C report check        only verify that every referenced graphic exists
#   make -C report warnings     build, then list LaTeX warnings from the log
#   make -C report view         open the PDF
#   make -C report clean        remove aux files, keep the PDF
#   make -C report distclean    remove aux files and the PDF
#
# The figures themselves are produced from the simulation output trees by
# macros/Makefile.report (`make -f macros/Makefile.report help` lists the
# per-group targets); this Makefile only typesets.  The PDF rule still depends
# on every file named by \includegraphics, so regenerating a figure rebuilds
# the PDF with no extra bookkeeping.
#
# Engine: latexmk is used when it is on PATH; otherwise the recipe falls back
# to repeated pdflatex passes until the .log stops asking for a rerun.  Force
# either with LATEXMK=/path/to/latexmk or LATEXMK= (empty => plain pdflatex).

# ---- locations ----------------------------------------------------------
# Absolute path of the directory holding this Makefile, so every target works
# from any working directory (`make -C report`, `cd report && make`, ...).
HERE    := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
TEXNAME := matrix_report
TEX     := $(HERE)/$(TEXNAME).tex
PDF     := $(HERE)/$(TEXNAME).pdf
LOG     := $(HERE)/$(TEXNAME).log

# ---- engine -------------------------------------------------------------
LATEXMK       ?= $(shell command -v latexmk 2>/dev/null)
PDFLATEX      ?= pdflatex
LATEXMK_FLAGS ?= -pdf -interaction=nonstopmode -halt-on-error -file-line-error
PDFLATEX_FLAGS ?= -interaction=nonstopmode -halt-on-error -file-line-error

ifeq ($(strip $(LATEXMK)),)
  # No latexmk: run pdflatex until the cross-references/table of contents settle
  # (three passes is the practical ceiling; the loop stops as soon as the log
  # no longer asks for a rerun).
  LATEX_CMD = cd $(HERE) && \
	for pass in 1 2 3; do \
	  echo "=== pdflatex (pass $$pass) ==="; \
	  $(PDFLATEX) $(PDFLATEX_FLAGS) $(TEXNAME).tex >/dev/null || \
	    { echo "pdflatex failed on pass $$pass; see $(TEXNAME).log"; exit 1; }; \
	  grep -qE 'Rerun to get|Label\(s\) may have changed|There were undefined (references|citations)' \
	    $(TEXNAME).log || break; \
	done
else
  LATEX_CMD = cd $(HERE) && $(LATEXMK) $(LATEXMK_FLAGS) $(TEXNAME).tex
endif

# ---- dependencies -------------------------------------------------------
# Every graphic pulled in by \includegraphics (paths are relative to HERE, as
# written in the .tex).  Kept separate from FIG_DEPS so `check` can report a
# missing file by name instead of make aborting with "no rule to make target".
FIG_REFS := $(shell grep -ho '\\includegraphics\(\[[^]]*\]\)\?{[^}]*}' $(TEX) 2>/dev/null \
              | grep -o '{[^}]*}$$' | tr -d '{}' | sort -u)
FIG_DEPS := $(wildcard $(addprefix $(HERE)/,$(FIG_REFS)))

CLEAN_AUX := $(addprefix $(HERE)/$(TEXNAME),.aux .log .out .toc .lof .lot .fls \
               .fdb_latexmk .synctex.gz .bbl .blg .nav .snm .vrb)

# Do not let make's built-in TeX/pattern rules second-guess the explicit rules.
.SUFFIXES:

.PHONY: all pdf figures check warnings view clean distclean help
.DEFAULT_GOAL := all

# ---- rules --------------------------------------------------------------
all pdf: $(PDF)

$(PDF): $(TEX) $(FIG_DEPS)
	@echo "=== typesetting $(TEXNAME).tex ==="
	@$(LATEX_CMD)
	@echo "=== wrote $(PDF) ==="

# Figures come from the data trees; delegate to the figure Makefile at the
# repo root.  Command-line overrides (MATRIX_DIR=..., WHOLE_DIR=..., ...) are
# passed through to the sub-make automatically.
figures:
	$(MAKE) -C $(HERE)/.. -f macros/Makefile.report figures

check:
	@missing=""; \
	for f in $(FIG_REFS); do \
	  [ -f "$(HERE)/$$f" ] || missing="$$missing $$f"; \
	done; \
	if [ -n "$$missing" ]; then \
	  echo "missing graphics:$$missing"; echo "run 'make -C $(HERE) figures' first"; exit 1; \
	fi; \
	echo "all $(words $(FIG_REFS)) referenced graphics present"

warnings: $(PDF)
	@grep -nE 'Overfull|Underfull|undefined (reference|citation)|LaTeX Warning' $(LOG) \
	  || echo "no LaTeX warnings in $(notdir $(LOG))"

view: $(PDF)
	@( xdg-open $(PDF) >/dev/null 2>&1 & )

clean:
	rm -f $(CLEAN_AUX)

distclean: clean
	rm -f $(PDF)

help:
	@echo "Report Makefile ($(HERE))"
	@echo "  all / pdf    typeset $(TEXNAME).pdf (default; figures untouched)"
	@echo "  figures      regenerate all figures via macros/Makefile.report"
	@echo "  check        verify every \\includegraphics file exists"
	@echo "  warnings     build, then list LaTeX warnings"
	@echo "  view         open the PDF"
	@echo "  clean        remove .aux/.log/.toc/... (keeps the PDF)"
	@echo "  distclean    clean + remove the PDF"
	@echo ""
	@if [ -n "$(strip $(LATEXMK))" ]; then \
	  echo "Engine: latexmk ($(LATEXMK))"; \
	else \
	  echo "Engine: pdflatex, repeated until refs settle"; \
	fi
