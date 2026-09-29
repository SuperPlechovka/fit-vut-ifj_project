# IFJ26 — vývojové cíle, nejsou potřeba v odevzdání.
# AI prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
PYTHON ?= python3
IC26INT ?= ic26int
TEAM_SIZE ?= 4
export IC26INT

.PHONY: unit system compile-tests test format format-check check pack infra-test
unit:
	$(PYTHON) tools/dev.py unit

system: all
	$(PYTHON) tools/dev.py system

compile-tests: all
	$(PYTHON) tools/dev.py system --compile-only

test: unit system

format:
	$(PYTHON) tools/dev.py format

format-check:
	$(PYTHON) tools/dev.py format-check

check: format-check
	$(PYTHON) tools/dev.py check

pack: all
	$(PYTHON) tools/pack.py --team-size $(TEAM_SIZE)

infra-test:
	$(PYTHON) -m unittest discover -s tools/tests -v

.PHONY: docs
docs:
	@mkdir -p build/docs
	@printf '%s\n' '# IFJ26 — API' '' 'Přehled modulů a rozhraní ze zdrojových komentářů.' > build/docs/api-index.md
	PROJECT_VERSION="$$(cat VERSION)" doxygen Doxyfile
	@test -s build/docs/html/index.html
