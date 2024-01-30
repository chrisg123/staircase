all:
	./build.sh

verbose:
	./build.sh --verbose

dist-verbose:
	./build.sh --dist --verbose

clean:
	rm -rf build/staircase

clean-dist:
	rm -rf build/dist

clean-all:
	rm -rf build
	rm -f .skip_initial_dependency_build

reset: clean-all
	rm -rf samples
	rm -f src/EmbeddedStepFile.h
	for dir in external/*; do \
		if [ -d "$$dir" ]; then \
			cd $$dir && git reset --hard HEAD && cd -; \
		fi \
	done

demo:
	./run_demo.sh

dist:
	./build.sh --dist

dist-debug:
	./build.sh --dist --debug

.PHONY: clean cleanall demo all verbose dist
