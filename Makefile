.PHONY: build

build:
	$(MAKE) -C ./Luma3DS_Mod
	mv ./Luma3DS_Mod/boot.firm boot.firm
	python3 pluginTest/run.py pluginTest/pluginTest.c

clean:
	$(MAKE) -C ./Luma3DS_Mod clean
	rm -rf pluginTest/*.cplg
	rm -rf pluginTest/*.o