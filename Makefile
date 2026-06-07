CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags sdl3)
LIBS = $(shell pkg-config --libs sdl3) -lm

TARGET = catclock-sdl3
SRCS = catclock_main.c catclock_tail.c catclock_eyes.c catclock_atlas.c catclock_hands.c
OBJS = $(SRCS:.c=.o)
WIN_OBJS = $(SRCS:.c=.win.o)

# Windows Cross-Compiler Target Directives
WIN_CC = x86_64-w64-mingw32-gcc
WIN_WINDRES = x86_64-w64-mingw32-windres
WIN_TARGET = catclock-sdl3.exe

# Base Windows compiler flags and libraries
WIN_LIBS = catclock-sdl3_resource.o -L$(WINDOWS_SDL_PREFIX)/lib -lSDL3 -lm -mwindows
WIN_CFLAGS = -Wall -Wextra -O2 -I$(WINDOWS_SDL_PREFIX)/include

# Verified official stable upstream production release asset
SDL_VER = 3.4.10
SDL_ZIP = SDL3-$(SDL_VER)-win32-x64.zip
SDL_ZIP_SIZE = 1161871
GITHUB_HOST = https://github.com
SDL_URL = $(GITHUB_HOST)/libsdl-org/SDL/releases/download/release-$(SDL_VER)/$(SDL_ZIP)
SDL_ZIP_SHA256 = 86e779617c6c2d4e20716e14a5bd9b1210595d62b88a05fed1ccd7fd53c902f9
SDL_DLL = SDL3.dll
SDL_DLL_SIZE = 2804736
SDL_DLL_SHA256 = c39fbda24eca1009b06a4d4e340d12511e3c8b0d44c4898d29e336e2cc7a25f0

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c catclock_shared.h
	$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule for cross-compilation objects
%.win.o: %.c catclock_shared.h
	$(WIN_CC) $(WIN_CFLAGS) -c $< -o $@

# Prevent GNU Make from deleting resource.rc and your specific icon asset as intermediates
.SECONDARY: resource.rc catclock_icon.ico

# Isolated generation rule: runs only if resource.rc is missing
resource.rc: assets/cat.xbm
	@echo "Processing transparent corner-triangle application icon assets..."
	# 1. Base workflow: Trim to bounding content box
	magick assets/cat.xbm -trim build_tmp_trimmed.png

	# 2. Apply geometric modifications to the native 48x48 footprint
	magick build_tmp_trimmed.png -shave 0x1 -background white -gravity West -extent 48x48 build_tmp_48base.png

	# 3. Add outline to 48
	magick build_tmp_48base.png \
		-fuzz 1% -fill none -draw "color 0,0 floodfill" \
		-fuzz 1% -fill none -draw "color 47,0 floodfill" \
		-fuzz 1% -fill none -draw "color 47,47 floodfill" \
		-fuzz 1% -fill none -draw "color 0,47 floodfill" \
		-fuzz 1% -fill none -draw "color 24,0 floodfill" \
		-stroke none -strokewidth 1 -draw "line 47,0 47,47" build_flat_master.png

	# 4. Outline and scale where required
	magick build_flat_master.png \
		\( +clone -alpha extract -morphology edgeout disk:2 -background white -alpha shape \) -compose dst_over -composite build_flat_48.png
	magick build_flat_48.png -scale 16x16 build_flat_16.png
	magick build_flat_48.png -scale 32x32 build_flat_32.png
	magick build_flat_48.png -scale 64x64 build_flat_64.png
	magick build_flat_48.png -scale 256x256 build_flat_256.png

	# 5. Bundle everything securely into the final .ico file
	magick build_flat_16.png build_flat_32.png build_flat_48.png build_flat_64.png build_flat_256.png catclock_icon.ico

	# 6. Compile the resource object layout
	echo '1 ICON "catclock_icon.ico"' > resource.rc
	rm -f build_tmp_*.png build_flat_*.png

# Target for building the Windows resource object file
catclock-sdl3_resource.o: resource.rc
	$(WIN_WINDRES) resource.rc -o catclock-sdl3_resource.o

# Cross-compilation target rule with verbose failure logging and download auto-resume
windows: catclock-sdl3_resource.o $(WIN_OBJS)
	@echo "Checking operational environment for $(SDL_DLL)..."
	@DLL_VALID=0; \
	if [ -f "$(SDL_DLL)" ]; then \
		DLL_SZ=$$(wc -c < "$(SDL_DLL)" | tr -d ' '); \
		if [ "$$DLL_SZ" -eq $(SDL_DLL_SIZE) ] && echo "$(SDL_DLL_SHA256)  $(SDL_DLL)" | sha256sum --check --status; then \
			DLL_VALID=1; \
		else \
			ACTUAL_HASH=$$(sha256sum "$(SDL_DLL)" | awk '{print $$1}'); \
			echo "Local $(SDL_DLL) is corrupt or invalid!"; \
			echo " -> Expected Size: $(SDL_DLL_SIZE) | Got: $$DLL_SZ"; \
		fi; \
	fi; \
	if [ "$$DLL_VALID" -ne 1 ]; then \
		echo "Procuring pristine asset validation package..."; \
		if [ -f "$(SDL_ZIP)" ]; then \
			ZIP_SZ=$$(wc -c < "$(SDL_ZIP)" | tr -d ' '); \
			if [ "$$ZIP_SZ" -ne $(SDL_ZIP_SIZE) ] || ! echo "$(SDL_ZIP_SHA256)  $(SDL_ZIP)" | sha256sum --check --status; then \
				echo "Cached archive is corrupt. Re-downloading..."; \
				curl -L -C - "$(SDL_URL)" -o "$(SDL_ZIP)"; \
			fi; \
		else \
			curl -L "$(SDL_URL)" -o "$(SDL_ZIP)"; \
		fi; \
		echo "Validating download checksum..."; \
		if ! echo "$(SDL_ZIP_SHA256)  $(SDL_ZIP)" | sha256sum --check --status; then \
			echo "FATAL: Downloaded archive failed cryptographic integrity check!"; \
			exit 1; \
		fi; \
		echo "Extracting runtime dependencies..."; \
		unzip -p "$(SDL_ZIP)" $(SDL_DLL) > "$(SDL_DLL)"; \
	fi; \
	echo "Compiling application objects for Windows..."
	@$(MAKE) $(WIN_OBJS)
	@echo "Linking final cross-compiled executable binary..."
	$(WIN_CC) $(WIN_OBJS) -o $(WIN_TARGET) $(WIN_LIBS)
	rm -f *.win.o

	@# Conditional Authenticode Code-Signing Sequence Check
	@if [ -f "cert.pfx" ]; then \
		echo "Detected cert.pfx file. Commencing Authenticode signing processing sequence..."; \
		if command -v osslsigncode >/dev/null 2>&1; then \
			osslsigncode sign -pkcs12 cert.pfx -pass password -n "Kit-Cat Widget Clock" -in $(WIN_TARGET) -out $(WIN_TARGET).signed && \
			mv $(WIN_TARGET).signed $(WIN_TARGET); \
			echo "Binary signature appended successfully."; \
		else \
			echo "Warning: cert.pfx found but 'osslsigncode' tool is missing from path environment!"; \
		fi; \
	else \
		echo "No cert.pfx signature asset found. Skipping code-signing phase."; \
	fi

	@echo "======================================================================"
	@echo "Done! Transfer '$(WIN_TARGET)' and 'SDL3.dll' to your Windows machine."
	@echo "======================================================================"

# Clean built build artifacts, object files, and resources
clean:
	rm -f $(OBJS) $(WIN_OBJS) $(TARGET) $(WIN_TARGET) *.png catclock-sdl3_resource.o
	rm -f  resource.rc catclock_icon.ico

# Clean heavy downloaded packages and extracted system DLLs
clean-dist: clean
	rm -f $(SDL_DLL) $(SDL_ZIP)

.PHONY: all windows clean clean-dist
