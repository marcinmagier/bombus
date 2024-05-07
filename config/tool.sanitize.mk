

SANITIZE_OPTIONS        = -fsanitize=address -fsanitize=undefined
SANITIZE_EXTRA_OPTIONS  = -fno-sanitize-recover -fno-omit-frame-pointer -fno-optimize-sibling-calls


CMAKE_CFLAGS_OPTIONS   += $(SANITIZE_OPTIONS) $(SANITIZE_EXTRA_OPTIONS)
CMAKE_CXXFLAGS_OPTIONS += $(SANITIZE_OPTIONS) $(SANITIZE_EXTRA_OPTIONS)
CMAKE_LDFLAGS_OPTIONS  += $(SANITIZE_OPTIONS)
CMAKE_TARGET_OPTIONS   += -DCMAKE_C_COMPILER="clang" -DCMAKE_CXX_COMPILER="clang++"

