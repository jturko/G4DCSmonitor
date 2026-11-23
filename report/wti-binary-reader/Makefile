MAIN = main
LATEXMK = latexmk
PYTHON = python3

.PHONY: all figures clean

all: $(MAIN).pdf

$(MAIN).pdf: $(MAIN).tex $(wildcard figures/*.pdf) $(wildcard figures/*.png)
	$(LATEXMK) -pdf -interaction=nonstopmode -halt-on-error $(MAIN).tex

figures:
	$(PYTHON) scripts/make_validation_figs.py

clean:
	$(LATEXMK) -C $(MAIN).tex
