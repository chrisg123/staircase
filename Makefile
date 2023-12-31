all:
	./build.sh

clean:
	rm -rf build/staircase

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

.PHONY: clean cleanall demo all
