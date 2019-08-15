#!/bin/bash

export DEPS_ROOT_LOCATON=$(grealpath .)
export VULKAN_SDK_VERSION="1.1.114.0"
export VULKAN_SDK_BASENAME="vulkansdk-macos-$VULKAN_SDK_VERSION"
export VULKAN_SDK="$DEPS_ROOT_LOCATON/$VULKAN_SDK_BASENAME/macOS"
export VULKAN_SDK_PKG_CONFIG_PATH="$VULKAN_SDK/lib/pkgconfig"
export VK_ICD_FILENAMES="$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json"
export VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layers.d"
export PATH="/usr/local/opt/python/libexec/bin:$VULKAN_SDK/bin:$PATH"

if [ ! -f "$VULKAN_SDK_BASENAME.tar.gz" ]; then
  wget https://sdk.lunarg.com/sdk/download/$VULKAN_SDK_VERSION/mac/$VULKAN_SDK_BASENAME.tar.gz
fi

if [ ! -d $VULKAN_SDK_BASENAME ]; then
 tar zxvf "$VULKAN_SDK_BASENAME.tar.gz"
 cp -R $VULKAN_SDK/include/libshaderc $VULKAN_SDK/include/shaderc
fi

if [ ! -d $VULKAN_SDK_PKG_CONFIG_PATH ]; then
  mkdir -p $VULKAN_SDK_PKG_CONFIG_PATH
fi

cat > "$VULKAN_SDK_PKG_CONFIG_PATH/vulkan.pc" << EOL
libdir=$VULKAN_SDK/lib
includedir=$VULKAN_SDK/include

Name: vulkan
Description: Vulkan
Version: $VULKAN_SDK_VERSION
Libs: -L\${libdir} -lMoltenVK
Libs.private: -lm
Cflags: -I\${includedir}
EOL

cat > "$VULKAN_SDK_PKG_CONFIG_PATH/shaderc.pc" << EOL
libdir=$VULKAN_SDK/lib
includedir=$VULKAN_SDK/include

Name: shaderc
Description: shaderc
Version: 2019.0.1
Libs: -L\${libdir} -lshaderc_shared
Cflags: -I\${includedir}
EOL


export PKG_CONFIG_PATH=$VULKAN_SDK_PKG_CONFIG_PATH:$PKG_CONFIG_PATH

if [ ! -d libplacebo ]; then
  git clone git@github.com:haasn/libplacebo.git

fi

if [ ! -d libplacebo-dist ]; then 
  cd libplacebo
  meson --prefix "$DEPS_ROOT_LOCATON/libplacebo-dist" ./build
  ninja -C./build
  ninja -C./build install
  cd ..
fi

export PKG_CONFIG_PATH="$DEPS_ROOT_LOCATON/libplacebo-dist/lib/pkgconfig":$PKG_CONFIG_PATH

echo "export PKG_CONFIG_PATH=$PKG_CONFIG_PATH"
