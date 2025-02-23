echobold () {
	echo "$(tput bold)" "$@" "$(tput sgr0)"
}

scriptDir=`dirname "$0"`

varMissing=false
checkHasValue() {
	varName="$1"
	varValue="$2"
	if [ -z "${varValue}" ]
	then
		echobold "$varName is not defined"
		varMissing=true
	fi
}

# If things aren't defined, but there's an "env.sh" right next to us, include it
if [ -z "${APPLE_TEAM_ID}"] && [ -f "$scriptDir/env.sh" ];
then
	set -a
	source "$scriptDir/env.sh"
	set +a
fi

checkHasValue "APPLE_TEAM_ID" "${APPLE_TEAM_ID}"
checkHasValue "APPLE_LOGIN_ID" "$APPLE_LOGIN_ID"
checkHasValue "APPLE_APP_PASSWORD" "$APPLE_APP_PASSWORD"
checkHasValue "APPLE_CODESIGN_APPLICATION" "$APPLE_CODESIGN_APPLICATION"
checkHasValue "APPLE_CODESIGN_INSTALLER" "$APPLE_CODESIGN_INSTALLER"
if [ "$varMissing" = true ];
then
	exit 1
fi

notaryToolCredentials="--team-id ${APPLE_TEAM_ID} --apple-id ${APPLE_LOGIN_ID} --password ${APPLE_APP_PASSWORD}"

signplugin () {
	target="$1"
	
	if [ ! -e "$target" ]
	then
		return 1
	fi

	if ! stapler validate -q "$target"
	then
		codesign --verbose --deep --force --timestamp --options runtime \
			--sign "${APPLE_CODESIGN_APPLICATION}" \
			"$target"
		codesign --verify --verbose --deep \
			"$target"

		/usr/bin/ditto -c -k --keepParent "$target" "$target.zip"

		if xcrun notarytool submit "$target.zip" \
			$notaryToolCredentials \
			--wait
		then
			echobold "Notarised $target"
			rm "$target.zip"
		else
			echobold "Notarising $target failed"
			rm "$target.zip"
			exit 1
		fi

		xcrun stapler staple "${target}"

		if stapler validate -q "${target}"
		then
			echo "$name signed and stapled"
		else
			echo "$name staple failed"
			exit 1
		fi
	fi
}

makepackage() {
	rootdir="$1"
	identifier="$2"
	packagePath="$3"
	
	pkgbuild --quiet \
		--root="$rootdir" \
		--install-location "/" \
		--sign "${APPLE_CODESIGN_INSTALLER}" \
		--timestamp \
		--identifier "$identifier" \
		"$packagePath"

	rm -rf "$rootdir"
}

if [ "$#" -lt 4 ]; then
	echo "Arguments: <installer.pkg> \"Bundle Name\" com.example.bundle.id <plugin/prefix>+"
	exit 1
fi

installerPath="$1"
shift
installerName="$1"
shift
installerId="$1"
shift

for prefix in "$@"
do
	if signplugin "$prefix.vst3"
	then
		mkdir -p "$installerPath.vst3-root/Library/Audio/Plug-ins/VST3$PLUGIN_INSTALL_SUBDIR"
		cp -r "$prefix.vst3" "$installerPath.vst3-root/Library/Audio/Plug-ins/VST3$PLUGIN_INSTALL_SUBDIR"
	fi

	if signplugin "$prefix.clap"
	then
		mkdir -p "$installerPath.clap-root/Library/Audio/Plug-ins/CLAP$PLUGIN_INSTALL_SUBDIR"
		cp -r "$prefix.clap" "$installerPath.clap-root/Library/Audio/Plug-ins/CLAP$PLUGIN_INSTALL_SUBDIR"
	fi
done

productbuild_packages=""
if [ -e "$installerPath.vst3-root" ]; then
	makepackage "$installerPath.vst3-root" "$installerId.vst3" "$installerPath.vst3.pkg"
	productbuild_packages="${productbuild_packages} --package $installerPath.vst3.pkg"
fi
if [ -e "$installerPath.clap-root" ]; then
	makepackage "$installerPath.clap-root" "$installerId.clap" "$installerPath.clap.pkg"
	productbuild_packages="${productbuild_packages} --package $installerPath.clap.pkg"
fi

resourceDir="$installerPath-resources"
if [ ! -d "$resourceDir" ]; then
	cp -r "$scriptDir/resources" "$resourceDir"
fi

productbuild --quiet \
	$productbuild_packages \
	--synthesize "$installerPath.generated.xml"

# Add custom stuff to the installer package
head -n 2 "$installerPath.generated.xml" > "$installerPath.xml"
customXmlFile="$resourceDir/distribution.custom.xml"
if [ ! -e "$resourceDir/distribution.custom.xml" ]; then
	# Generate custom distribution.xml
	cat > "$customXmlFile" <<EOF
<title>$installerName</title>
<!-- a square logo padded to 182x220 fits nicely in the corner -->
<!-- otherwise, here's a background based on Alexey Ruban's photo: https://unsplash.com/photos/73o_FzZ5x-w -->
<background file="background-light.png" alignment="bottomleft" scaling="proportional" />
<background-darkAqua file="background-dark.png" />

<welcome file="welcome.html"/>
<readme file="readme.html"/>
<license file="license.html"/>
<conclusion file="conclusion.html"/>

<!-- build settings -->
<domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="true" />
EOF
fi

cat "$resourceDir/distribution.custom.xml" >> "$installerPath.xml"

# Generate welcome/conclusion if they don't exist
if [ ! -e "$resourceDir/welcome.html" ]; then
	echo -e "<html><body><br>\n<p style=\"margin: 0px; font: 14px 'Lucida Grande'\">This will install  <strong>$installerName</strong>.</p>\n</body></html>" > "$resourceDir/welcome.html"
fi
if [ ! -e "$resourceDir/conclusion.html" ]; then
	echo -e "<html><body><br>\n<p style=\"margin: 0px; font: 14px 'Lucida Grande'\">Finished installing <strong>$installerName</strong>.</p>\n</body></html>" > "$resourceDir/conclusion.html"
fi

# The rest of the generated file, with some changes
tail -n +3 "$installerPath.generated.xml" \
	| sed -e 's/<line choice="default">//' -e 's/<\/line>//' \
	| sed -E 's/<choice id=".*\.clap"/& title="CLAP"/' \
	| sed -E 's/<choice id=".*\.vst3"/& title="VST3"/' \
	| sed -e 's/visible="false"/visible="true"/' \
	| sed -e 's/customize="never"/customize="always"/' \
	>> "$installerPath.xml"
	
installerDir=`dirname "$installerPath"`
installerFile=`basename "$installerPath"`
pushd "$installerDir"

if ! productbuild --quiet \
	--distribution "$installerFile.xml" \
	--resources "$installerFile-resources" \
	--identifier "$installerId" \
	--sign "${APPLE_CODESIGN_INSTALLER}" \
	--timestamp \
	"$installerFile"
then
	exit 1
fi

popd

rm -rf $installerPath.*

echobold "Generated installer PKG"

if xcrun notarytool submit "$installerPath" \
	$notaryToolCredentials \
	--wait
then
	echobold "Notarised $installerPath"
	xcrun stapler staple "${installerPath}"
else
	echobold "Notarising $installerPath failed"
	rm "$installerPath"
	exit 1
fi

if ! spctl --assess --ignore-cache --verbose --type install "$installerPath"
then
	echo "spctl failed $installerPath"
	exit 1
fi
if ! pkgutil --verbose --check-signature "$installerPath"
then
	echo "pkgutil failed $installerPath"
fi
