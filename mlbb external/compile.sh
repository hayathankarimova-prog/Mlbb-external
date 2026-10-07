cat << 'EOF' > compile.sh
#!/ Melliora compile script for Termux root
echo "[*] Starting Termux Vulkan Compilation..."

# Эски убактылуу файлдарды тазалоо
rm -rf jni/obj jni/libs libs obj

# NDK аркылуу кураштыруу буйругу (Биздин Application.mk жана Android.mk файлдарын колдонот)
ndk-build NDK_PROJECT_PATH=. NDK_APPLICATION_MK=jni/Application.mk APP_BUILD_SCRIPT=jni/Android.mk

if [ $? -eq 0 ]; then
    echo "[+] Compilation Successful!"
    echo "[+] Your executable is ready at: ./libs/arm64-v8a/rsa"
    # Файлга иштөө уруксатын берүү
    chmod +x libs/arm64-v8a/rsa
else
    echo "[-] Compilation Failed. Check the errors above."
fi
EOF
