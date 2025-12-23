# Custom Makefile targets for Gambit
# This file takes precedence over the CMake-generated Makefile


.PHONY: help build test clean run-server run-client dev test-coverage test-coverage-open pre-commit
help: ## Show this help message
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'


build:  ## Builds the project as is
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make

clean:  ## Remove build artifacts for a fresh rebuild
	@if [ -d "build" ]; then \
		rm -rf build && \
		echo "Build artifacts cleaned!"; \
	else \
		echo "No build directory found - nothing to clean."; \
	fi

test: build  ## Builds and runs tests
	./build/Tests

test-coverage:  ## Run tests with code coverage and generate HTML report
	@echo "Building with coverage enabled..."
	@mkdir -p build
	@cd build && \
	cmake -DENABLE_COVERAGE=ON .. && \
	make Tests
	@echo "Running tests..."
	@cd build && ./Tests
	@echo "Generating coverage report..."
	@mkdir -p build/coverage
	@cd build && \
	lcov --capture --directory . --output-file coverage.info --ignore-errors inconsistent,unsupported --rc branch_coverage=1 && \
	lcov --remove coverage.info '/usr/*' '/opt/*' '*/build/*' --output-file coverage.info --ignore-errors inconsistent,unused,unsupported --rc branch_coverage=1 && \
	genhtml coverage.info --output-directory coverage --ignore-errors inconsistent,category,source,unused,deprecated,unsupported
	@echo ""
	@echo "✓ Coverage report generated: build/coverage/index.html"
	@echo "  Run 'make test-coverage-open' to view in browser"

test-coverage-open: test-coverage  ## Generate coverage report and open in browser
	@open build/coverage/index.html

run-server: build  ## Start the Gambit server on 0.0.0.0:1234
	@if [ ! -f "build/Server" ]; then \
		echo "Error: Server executable not found. Run 'make build' first."; \
		exit 1; \
	fi
	@echo "Starting Gambit Server on 0.0.0.0:1234..."
	./build/Server

run-client: build  ## Start a single Gambit client
	@if [ ! -f "build/Client" ]; then \
		echo "Error: Client executable not found. Run 'make build' first."; \
		exit 1; \
	fi
	@echo "Starting Gambit Client (connecting to 127.0.0.1:1234)..."
	./build/Client

dev: build  ## Full dev environment: 1 server + 4 clients
	@./dev.sh


.PHONY: pre-commit
pre-commit: ## Runs pre-commit hooks like format and lint
	uv run pre-commit run -a
