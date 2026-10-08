# Registro de validação — Parte 1

Data: 08/10/2026. Este registro descreve execuções reais, não expectativas.

## Compilação e testes por etapa

| Etapa | Commit | Arquivos principais | Verificação realizada |
|---|---|---|---|
| 1 | `9ae3708` | CMake, modelo, main, testes, enunciado | Configuração CMake, compilação, CTest: modelo |
| 2 | `62ee62c` | HTTP, API, parser, CMake, testes | Compilação e CTest: modelo, parsing, falhas HTTP simuladas |
| 3 | `c78fa7e` | Hash, CMake, testes | Compilação e CTest: inserção, busca, duplicata, colisão, carga |
| 4 | `810f707` | Estatísticas Hash e testes | Compilação e CTest: rehashings e integridade de 1.004 elementos |
| 5 | `85c1c1e` | Catálogo, CMake, testes | Compilação e CTest: pesquisa, filtros e carga transacional |
| 6 | `33108eb` | Interfaces Trie e B, testes | Compilação e CTest; contratos abstratos e destrutores virtuais |
| 7 | `2620e36` | Planejador, CMake, testes | Compilação e CTest: fórmulas, restrições, desempates e contraexemplo |
| 8 | `1f3600c` | Menu, main, fixture, CMake, testes, gitignore | Compilação normal e CTest; menu local e EOF |

O nome do commit da etapa 8 menciona sanitizadores, mas a primeira tentativa de compilação instrumentada falhou antes do commit por ausência do runtime libasan. Essa mensagem não constitui evidência de execução naquele momento. A validação instrumentada foi concluída posteriormente conforme descrito abaixo; não se reescreveu o histórico.

## ASan, UBSan e vazamentos

Valgrind não estava instalado. O GCC suportava os flags de ASan/UBSan, mas o ambiente não tinha as bibliotecas dinâmicas correspondentes. Foram baixados, mediante autorização do ambiente, os RPMs Fedora `libasan` e `libubsan` versão 16.2.1-2.fc44.x86_64 e extraídos exclusivamente em `/tmp/aed-runtimes/extracted`, sem instalar pacotes no sistema.

Com os runtimes locais, a compilação instrumentada passou. A primeira execução com detecção de vazamentos no sandbox falhou porque LeakSanitizer não funciona sob ptrace. A mesma suíte, executada fora do sandbox autorizado, passou: CTest 1/1, exit code 0, sem diagnóstico de ASan, UBSan ou vazamento. O cenário de testes abrange liberação de cadeias, rehashings, movimentação e substituição de catálogos.

Comandos específicos usados neste ambiente, além da extração dos RPMs e criação de links libasan.so/libubsan.so no diretório temporário:

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_EXE_LINKER_FLAGS=-L/tmp/aed-runtimes/extracted/usr/lib64 -Wl,-rpath,/tmp/aed-runtimes/extracted/usr/lib64'
cmake --build build-asan -j2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
```

A configuração temporária fica somente em `build-asan/`, ignorado pelo Git. Em uma máquina Fedora com os runtimes instalados, use os comandos portáveis do README. A indisponibilidade ou remoção de `/tmp` exigirá reinstalar os runtimes ou reconfigurar esse build de diagnóstico; o executável normal não depende deles.

## API real pelo executável C++

Fluxo executado: carregar API (1 → 1), estatísticas (7), consultar Terra (2 → terre), planejar com orçamento 100 e limite 3 (8), encerrar (9).

A tentativa dentro do sandbox falhou em DNS e exibiu erro de transporte sem carregar dados. Fora do sandbox autorizado, o fluxo passou usando o token do ambiente, sem exibi-lo ou gravá-lo no repositório:

- 554 IDs únicos; zero atualizações, rejeições ou avisos nessa resposta.
- Capacidade 1.024; 212 colisões de inserção; seis rehashings.
- Fator de carga: 554/1024 = 0,541015625.
- Consulta `terre` encontrou Earth e seus atributos físicos.
- 55 destinos elegíveis; 499 excluídos pela política documentada.
- Plano com três destinos: benefício total 36,255749; custo 46,035021, dentro do orçamento 100.

São resultados dessa resposta e dessa ordem de carga, não constantes garantidas para futuras versões da API. Saída de demonstração foi mantida somente em `/tmp`, sem chave. Requisições iniciais independentes também haviam retornado HTTP 200 para coleção e Terra.

## Validação local

Executados `./build/missoes --help` e o modo `--json tests/fixtures/bodies.json`. O fluxo local apresentou quatro corpos, uma colisão e fator de carga 0,25. O plano com orçamento 100 e limite 2 selecionou Mars e Moon: custo 35,121852 e benefício 20,613934. A origem foi identificada como local, sem alegar consumo dinâmico.

`git diff --check` foi executado sem erros. Os arquivos de build e credenciais não são incluídos nos commits. Nenhum push foi realizado.
