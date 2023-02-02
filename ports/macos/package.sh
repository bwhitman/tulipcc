# package.sh
# packages the Tulip Deskop binary into a macOS app, universal binary, codesigns for distribution

# Note : on mac you need both types of homebrew, intel and AS
# https://medium.com/mkdir-awesome/how-to-install-x86-64-homebrew-packages-on-apple-m1-macbook-54ba295230f
# arch -x86_64 /usr/local/homebrew/bin/brew install libffi 

# Build for arm then intel
make WHICH_ARCH=arm64 clean
make WHICH_ARCH=arm64
make WHICH_ARCH=x86_64 clean
make WHICH_ARCH=x86_64
# lipo them together
lipo -create -output tulip tulip.x86_64 tulip.arm64
rm tulip.x86_64
rm tulip.arm64
mkdir -p dist

# Now make the app bundle
rm -rf dist/Tulip\ CC.app
mkdir -p dist/Tulip\ CC.app/Contents/{MacOS,Resources,Frameworks,libs}
cp tulip dist/Tulip\ CC.app/Contents/MacOS
cp Info.plist dist/Tulip\ CC.app/Contents/
cp -rf ../../tulip_home dist/Tulip\ CC.app/Contents/Resources/
cp -a SDL2.framework dist/Tulip\ CC.app/Contents/Frameworks/
cp libs/libsoundio.2.0.0.dylib dist/Tulip\ CC.app/Contents/libs/
install_name_tool -add_rpath @executable_path/../Frameworks dist/Tulip\ CC.app/Contents/MacOS/tulip
install_name_tool -change "@@HOMEBREW_PREFIX@@/opt/libsoundio/lib/libsoundio.2.dylib" "@executable_path/../libs/libsoundio.2.0.0.dylib" "dist/Tulip CC.app/Contents/MacOS/tulip"
cp tulip.icns dist/Tulip\ CC.app/Contents/Resources/
codesign -s "Developer ID Application: Brian Whitman (Y6CQ3JU8G4)" dist/Tulip\ CC.app/Contents/libs/libsoundio.2.0.0.dylib -f
codesign -s "Developer ID Application: Brian Whitman (Y6CQ3JU8G4)" dist/Tulip\ CC.app/Contents/Frameworks/SDL2.framework/Versions/A/SDL2 -f
codesign -s "Developer ID Application: Brian Whitman (Y6CQ3JU8G4)" dist/Tulip\ CC.app/Contents/MacOS/tulip -f

# I then run "AppWrapper 4.0" to notarize the package. I would love to convert that into a command line tool, i believe xcrun notarytool
# TODO: https://github.com/mitchellh/gon

 

