# Helper Makefile for Fedora COPR custom build method
# COPR executes: make -f .copr/Makefile srpm outdir="..." spec="..."

.PHONY: srpm

outdir ?= .
spec ?= cdrive.spec

srpm:
	@mkdir -p $(outdir)
	@version=$$(grep 'CDRIVE_VERSION' cdrive.h | cut -d'"' -f2); \
	if [ -z "$$version" ]; then version="1.0.3"; fi; \
	echo "Building source archive for cdrive $$version..."; \
	tar --exclude='./.git' --exclude='./out' --exclude='./sysroot' --exclude='./*.rpm' \
		--transform "s,^\.,CDrive-$$version," \
		-czf $(outdir)/cdrive-$$version.tar.gz . ; \
	rpmbuild -bs \
		--define "_sourcedir $(outdir)" \
		--define "_srcrpmdir $(outdir)" \
		$(spec)
