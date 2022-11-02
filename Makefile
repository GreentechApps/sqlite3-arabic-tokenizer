prepare-dist:
	rm -rf dist
	mkdir dist
	rm -rf generate
	mkdir generate

download-sqlite:
	curl -L https://www.sqlite.org/2022/sqlite-amalgamation-3390400.zip --output sqlite3.zip # change sql version if need
	unzip sqlite3.zip
	mv sqlite-amalgamation-3390400/* generate  # change sql version if need
	cp src/*.c generate
	rm -rf sqlite-amalgamation-3390400  # change sql version if need
	rm -rf sqlite3.zip


compile-linux:
	gcc -fPIC -shared generate/arabic_fts5_tokenizer.c -o dist/arabic_fts5_tokenizer.so

pack-linux:
	zip -j dist/arabic_fts5_tokenizer-linux-x86.zip dist/*.so

compile-windows:
	gcc -shared -I. generate/arabic_fts5_tokenizer.c -o dist/arabic_fts5_tokenizer.dll

pack-windows:
	7z a -tzip dist/arabic_fts5_tokenizer-win-x64.zip ./dist/*.dll

compile-macos:
	gcc -fPIC -dynamiclib -I generate generate/arabic_fts5_tokenizer.c -o dist/arabic_fts5_tokenizer.dylib

compile-macos-x86:
	mkdir -p dist/x86
	gcc -fPIC -dynamiclib -I generate src/arabic_fts5_tokenizer.c -o dist/x86/arabic_fts5_tokenizer.dylib -target x86_64-apple-macos10.12

compile-macos-arm64:
	mkdir -p dist/arm64
	gcc -fPIC -dynamiclib -I generate generate/arabic_fts5_tokenizer.c -o dist/arm64/arabic_fts5_tokenizer.dylib -target arm64-apple-macos11

pack-macos:
	zip -j dist/arabic_fts5_tokenizer-macos-x86.zip dist/x86/*.dylib
	zip -j dist/arabic_fts5_tokenizer-macos-arm64.zip dist/arm64/*.dylib

end-dist:
	rm -rf generate