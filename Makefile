.ONESHELL:

.PHONY: help build test
help: ## Show this help message
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'


build:  ## Builds the project as is
		mkdir -p build && \
		cd build && \
		cmake .. && \
		make

test: build  ## Builds and runs tests
	./build/Tests
