./build_host/gba_recompile \
    --rom /chemin/vers/mario_kart_super_circuit_usa.gba \
    --config game.toml \
    --config symbols/AMKE_symbols.toml \
    --symbols symbols/imported_symbols.tsv \
    --data-symbols symbols/imported_data_symbols.tsv \
    --out generated \
    --max-functions 65536
