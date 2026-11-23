# Makefile for the CLYC efficiency report (main.tex -> main.pdf).
#
# Usage:
#   make                     typeset main.pdf (default goal)
#   make pdf                 same as above
#   make figures             regenerate all analysis data and figures via ROOT
#   make data                run analyze_exp_spectra.C (peak fits & resolution model)
#   make distance-figs       run make_distance_figs.C (distance scan plots)
#   make exp-sim-figs        run make_exp_sim_figs.C (200 mm spectral overlays)
#   make flush-figs          run make_flush_comparison.C (flush source vs CSV)
#   make check               verify that all graphics referenced in main.tex exist
#   make warnings            typeset and list LaTeX warnings from main.log
#   make view                open main.pdf with xdg-open
#   make clean               remove LaTeX auxiliary files (keeps main.pdf)
#   make distclean           remove auxiliary files and main.pdf
#   make help                show this help message
#
# Works both from report/eff_clyc and via `make -C report/eff_clyc`.

# ---- paths --------------------------------------------------------------
HERE := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
REPO := $(abspath $(HERE)/../..)

TEXNAME := main
TEX     := $(HERE)/$(TEXNAME).tex
PDF     := $(HERE)/$(TEXNAME).pdf
LOG     := $(HERE)/$(TEXNAME).log

# Additional TeX source inputs
EXTRA_TEX := $(wildcard $(HERE)/setup_diagram.tex)

# ---- engine -------------------------------------------------------------
LATEXMK        ?= $(shell command -v latexmk 2>/dev/null)
PDFLATEX       ?= pdflatex
LATEXMK_FLAGS  ?= -pdf -interaction=nonstopmode -halt-on-error -file-line-error
PDFLATEX_FLAGS ?= -interaction=nonstopmode -halt-on-error -file-line-error

ifeq ($(strip $(LATEXMK)),)
  # Fallback when latexmk is not installed: run pdflatex up to 3 times
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
# Extract figure filenames from \includegraphics (paths are relative to figures/)
FIG_REFS := $(shell grep -ho '\\includegraphics\(\[[^]]*\]\)\?{[^}]*}' $(TEX) 2>/dev/null \
              | grep -o '{[^}]*}$$' | tr -d '{}' | sort -u)
FIG_DEPS := $(wildcard $(addprefix $(HERE)/figures/,$(FIG_REFS)))

CLEAN_AUX := $(addprefix $(HERE)/$(TEXNAME),.aux .log .out .toc .lof .lot .fls \
               .fdb_latexmk .synctex.gz .bbl .blg .nav .snm .vrb)

.SUFFIXES:

.PHONY: all pdf figures data distance-figs exp-sim-figs flush-figs \
        check warnings view clean distclean help

.DEFAULT_GOAL := all

# ---- typeset rules ------------------------------------------------------
all pdf: $(PDF)

$(PDF): $(TEX) $(EXTRA_TEX) $(FIG_DEPS)
	@echo "=== typesetting $(TEXNAME).tex ==="
	@$(LATEX_CMD)
	@echo "=== wrote $(PDF) ==="

# ---- analysis & figure generation ---------------------------------------
data:
	@echo "=== analyzing experimental spectra and resolution ==="
	cd $(REPO) && root -l -b -q report/eff_clyc/scripts/analyze_exp_spectra.C

distance-figs: data
	@echo "=== generating distance comparison figures ==="
	cd $(REPO) && root -l -b -q report/eff_clyc/scripts/make_distance_figs.C

exp-sim-figs: data
	@echo "=== generating 200 mm exp vs sim figures ==="
	cd $(REPO) && root -l -b -q report/eff_clyc/scripts/make_exp_sim_figs.C

flush-figs:
	@echo "=== generating flush source comparison figures ==="
	cd $(REPO) && root -l -b -q report/eff_clyc/scripts/make_flush_comparison.C

figures: data distance-figs exp-sim-figs flush-figs
	@echo "=== all analysis figures updated ==="

# ---- verification & inspection ------------------------------------------
check:
	@missing=""; \
	for f in $(FIG_REFS); do \
	  [ -f "$(HERE)/figures/$$f" ] || missing="$$missing $$f"; \
	done; \
	if [ -n "$$missing" ]; then \
	  echo "missing graphics:$$missing"; echo "run 'make figures' first"; exit 1; \
	fi; \
	echo "all $(words $(FIG_REFS)) referenced graphics present in figures/"

warnings: $(PDF)
	@grep -nE 'Overfull|Underfull|undefined (reference|citation)|LaTeX Warning' $(LOG) \
	  || echo "no LaTeX warnings in $(notdir $(LOG))"

view: $(PDF)
	@( xdg-open $(PDF) >/dev/null 2>&1 & )

# ---- clean rules --------------------------------------------------------
clean:
	rm -f $(CLEAN_AUX) $(HERE)/figures/*_raw.pdf

distclean: clean
	rm -f $(PDF)

help:
	@echo "Report Makefile ($(HERE))"
	@echo "  make [all|pdf]     typeset $(TEXNAME).pdf (default)"
	@echo "  make figures       regenerate all data & figures via ROOT"
	@echo "  make data          run analyze_exp_spectra.C"
	@echo "  make distance-figs run make_distance_figs.C"
	@echo "  make exp-sim-figs  run make_exp_sim_figs.C"
	@echo "  make flush-figs    run make_flush_comparison.C"
	@echo "  make check         verify all referenced graphics exist"
	@echo "  make warnings      typeset and list LaTeX warnings"
	@echo "  make view          open $(TEXNAME).pdf"
	@echo "  make clean         remove LaTeX auxiliary files"
	@echo "  make distclean     clean + remove $(TEXNAME).pdf"
	@echo ""
	@if [ -n "$(strip $(LATEXMK))" ]; then \
	  echo "LaTeX Engine: latexmk ($(LATEXMK))"; \
	else \
	  echo "LaTeX Engine: pdflatex (iterative)"; \
	fi
