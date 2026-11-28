MAIN = main
LATEXMK = latexmk
ROOT ?= root
P1_MAX_ENTRIES ?= 2000000
REPO := $(abspath ../..)

.PHONY: all figures data p1maps p1fig p1energy clean

all: figures
	$(LATEXMK) -pdf -interaction=nonstopmode -halt-on-error $(MAIN).tex

$(MAIN).pdf: $(MAIN).tex $(wildcard figures/*.pdf) $(wildcard figures/*.png)
	$(LATEXMK) -pdf -interaction=nonstopmode -halt-on-error $(MAIN).tex

figures:
	cd $(REPO) && $(ROOT) -l -b -q 'scripts/wssa/validate_wssa_root.C+'

data:
	cd $(REPO) && $(ROOT) -l -b -q 'scripts/wssa/run_all_wssa.C+'

# Needs P1_BASE_DIR pointing at the SurfaceFlux_v3 data root.
p1maps:
	cd $(REPO) && P1_MAX_ENTRIES=$(P1_MAX_ENTRIES) $(ROOT) -l -b -q 'scripts/wssa/make_p1_phi_z_maps.C+'

p1fig: p1maps
	cd $(REPO) && $(ROOT) -l -b -q 'report/wti-binary-reader/scripts/make_p1_phi_z_fig.C'

p1energy: p1maps
	cd $(REPO) && $(ROOT) -l -b -q 'report/wti-binary-reader/scripts/make_p1_energy_compare.C+'

clean:
	$(LATEXMK) -C $(MAIN).tex
