# Use lightweight Linux
FROM debian:stable-slim

# Install build dependencies
RUN apt-get update && apt-get install -y \
	build-essential \
	libreadline-dev \
	&& rm -rf /var/lib/apt/lists/*

# Set working dir
WORKDIR /app

# Copy source
COPY . .

# Build
RUN make clean && make

# Run shell by default
CMD ["./my_shell"]
