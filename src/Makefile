#
ifndef GEM_ROOT
$(error GEM_ROOT must be set -- Please fix the environment...)
endif

all:
	(cd wvServer; make)
	(cd EGADS/src; make)
	(cd EGADS/examples; make -f egads2cart.make)
	(cd EGADS/examples; make -f vGeom.make)
	(cd EGADS/examples; make -f vTess.make)
	(cd OpenCSM; make)
	@echo " "
	@echo " *** Completed! ***"

clean:
	(cd wvServer; make clean)
	(cd EGADS/src; make clean)
	(cd EGADS/examples; make -f egads2cart.make clean)
	(cd EGADS/examples; make -f vGeom.make clean)
	(cd EGADS/examples; make -f vTess.make clean)
	(cd OpenCSM; make clean)

cleanall:
	(cd wvServer; make cleanall)
	(cd EGADS/src; make cleanall)
	(cd EGADS/examples; make -f egads2cart.make cleanall)
	(cd EGADS/examples; make -f vGeom.make cleanall)
	(cd EGADS/examples; make -f vTess.make cleanall)
	(cd OpenCSM; make cleanall)
