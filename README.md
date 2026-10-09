# Crônicas do Espaço — Planejamento Algorítmico de Missões

Projeto da Parte 1 de Algoritmos e Estruturas de Dados II da Universidade Federal de Pelotas (UFPel). O sistema consulta dados reais do Sistema Solar, organiza corpos celestes em uma Tabela Hash própria e executa uma heurística gulosa para selecionar destinos sob recursos limitados.

## Integrantes

- Ian Xavier Freitas.
- Caetano Seixas Blanke.

## Escopo e fonte principal

O enunciado está em [docs/enunciado.pdf](docs/enunciado.pdf). A Parte 1 implementa apenas a Hash; Trie e Árvore B têm interfaces abstratas para a Parte 2. A opção gulosa escolhida é A — Planejamento e Triagem de Missões. O README constitui o relatório técnico consolidado, incluindo análise amortizada.

## Ferramentas e instalação no Fedora

C++17, CMake, libcurl e nlohmann-json. Não há download implícito de dependências pelo CMake.

```bash
sudo dnf install gcc-c++ cmake libcurl-devel nlohmann-json-devel
# Opcional: diagnósticos de memória
sudo dnf install libasan libubsan valgrind
```

O ambiente inicial disponibilizou GCC 16.2.1, CMake 4.3.0, libcurl 8.18.0 e configuração CMake do nlohmann-json 3.12.0. O pacote de desenvolvimento nlohmann-json não constava no RPM local, mas seu cabeçalho e configuração CMake funcionaram na compilação.

## Compilação, testes e execução

Na raiz do repositório:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/missoes
```

Para ver cada grupo de testes, execute `./build/cronicas_tests`. O CTest registra uma suíte, que reúne os grupos internos.

```bash
./build/missoes --help
# Modo local explícito: não efetua aquisição dinâmica
./build/missoes --json tests/fixtures/bodies.json
```

O arquivo de fixture contém um pequeno conjunto didático, com um corpo sintético para testar ausência de informações. Ele não representa a coleção atual inteira da API e não é apresentado como aquisição dinâmica.

## Configuração segura da API

Obtenha um token em https://api.le-systeme-solaire.net/generatekey.html. Em Bash, a entrada abaixo não ecoa o valor nem o coloca no histórico como texto literal:

```bash
read -r -s -p 'SOLAR_API_KEY: ' SOLAR_API_KEY
printf '\n'
export SOLAR_API_KEY
./build/missoes
unset SOLAR_API_KEY
```

O programa lê a variável somente ao carregar a API. Não imprime cabeçalhos, token nem corpo bruto de erros HTTP. A opção local funciona sem chave. `.env`, chaves e diretórios de segredos estão ignorados; o programa não lê `.env` automaticamente. Nunca versionar credenciais.

## Fonte de dados e validação

API: **The Solar System OpenData**, https://api.le-systeme-solaire.net/.
Documentação oficial: https://api.le-systeme-solaire.net/ e https://api.le-systeme-solaire.net/swagger/.
Consulta documental e validação autenticada: **08/10/2026**.

A escolha se justifica pela diversidade de planetas, luas, planetas anões, asteroides e cometas e por seus atributos físicos. Foram observados HTTP 200 nos endpoints de coleção e Terra durante a validação inicial: 554 registros; seis categorias, incluindo Star. A disponibilidade e os valores podem mudar.

| Endpoint | Uso |
|---|---|
| `GET /rest/bodies/` | Aquisição da coleção pelo programa |
| `GET /rest/bodies/terre` | Exemplo validado e documentado; consultas do programa após a carga são locais |

O cabeçalho é `Authorization: Bearer <token>`. Exemplo sem inserir a chave literalmente no comando nem no argumento de cabeçalho do processo curl:

```bash
printf 'Authorization: Bearer %s\n' "$SOLAR_API_KEY" |
  curl --fail --silent --show-error --header @- \
  'https://api.le-systeme-solaire.net/rest/bodies/'
```

Para consultar o exemplo individual, troque a URL por `https://api.le-systeme-solaire.net/rest/bodies/terre`. A aplicação não depende desse segundo endpoint para consultas locais.

Exemplo reduzido da coleção:

```json
{
  "bodies": [{
    "id": "terre",
    "englishName": "Earth",
    "bodyType": "Planet",
    "isPlanet": true,
    "gravity": 9.80665,
    "meanRadius": 6371.0084,
    "mass": {"massValue": 5.97237, "massExponent": 24},
    "semimajorAxis": 149598023,
    "avgTemp": 288
  }]
}
```

A resposta individual é diretamente o objeto do corpo; o parser da aplicação espera a coleção com `bodies`. A documentação pública apresenta referências antigas a `rowData`, embora o histórico registre sua remoção; esse parâmetro não é utilizado.

Na coleção observada havia 295 massas nulas, 494 gravidades zero, 206 raios zero e 542 temperaturas zero. O programa preserva zeros recebidos e informa que podem representar indisponibilidade; não transforma ausência em zero. Em particular, zero de gravidade ou raio não será aceito para estimar missão.

## Modelagem

`CelestialBody` contém:

| Campo | Representação / unidade |
|---|---|
| `id` | String obrigatória, não vazia; chave exata da API |
| `englishName` | String; na ausência, exibição usa ID |
| `bodyType` | String; aceita categorias futuras para consulta, sem inventar peso de missão |
| `isPlanet` | `optional<bool>`; ausência difere de false |
| `gravity` | `optional<double>`, m/s² |
| `meanRadius` | `optional<double>`, km |
| `semimajorAxis` | `optional<double>`, km, parâmetro orbital |
| `avgTemp` | `optional<double>`, K |
| `mass` | `optional<Mass>`; mantissa e expoente decimal em kg |

Massa é mantida em notação científica, sem expandir potências desnecessariamente. Números físicos negativos, não finitos ou de tipo incorreto geram aviso e ficam ausentes. Massa exige mantissa positiva e expoente inteiro representável. Raiz inválida ou JSON malformado causam erro; registros sem ID válido são rejeitados e contabilizados. Atributos não utilizados são descartados.

## Arquitetura e arquivos

| Arquivo / diretório | Responsabilidade |
|---|---|
| `include/cronicas/`, `src/` | Contratos e implementação dos módulos |
| `CelestialBody` | Modelo e nome de exibição |
| `HttpClient` | Contrato HTTP e implementação libcurl |
| `SolarApiClient` | Token do ambiente, endpoint, status HTTP e JSON local |
| `BodyJsonParser` | Validação e conversão JSON |
| `HashTable` | Nós próprios, busca, inserção, atualização, memória e métricas |
| `BodyCatalog` | Carga transacional, pesquisas e filtros |
| `TrieIndex`, `BTreeIndex` | Interfaces abstratas da Parte 2 |
| `MissionPlanner` | Estimativas físicas e seleção gulosa |
| `TerminalUI`, `main.cpp` | Menu, leitura e apresentação |
| `tests/tests.cpp` | Suíte sem dependência de rede |
| `tests/fixtures/bodies.json` | Entrada local de teste |
| `docs/arguicao.md` | Demonstração e perguntas para defesa |
| `docs/validacao.md` | Registro dos resultados executados |
| `data/` | Local sugerido para dados locais próprios; não exige um snapshot versionado |

A libcurl é encapsulada por um contrato simples que permite substituir o transporte por um cliente simulado nos testes. Não há dependência de framework de testes.

O HTTP verifica TLS, limita a resposta a 16 MiB e utiliza timeouts de conexão de 10 s e total de 30 s. Redirecionamentos não são seguidos para evitar encaminhamento de credenciais a outro host. Erros de transporte, autenticação, limite de requisições, outros status e parsing são comunicados. Não há repetição automática nem substituição silenciosa por JSON local.

A carga constrói uma Hash temporária e somente então substitui o catálogo atual. Falha de aquisição, parsing, alocação ou carga vazia preserva o catálogo anterior. Campos defeituosos opcionais permitem carga parcial com avisos; não há garantia de completude quando registros são rejeitados.

Armazenamento local significa objetos na memória da aplicação após a aquisição, como permitido pelo enunciado. Não há cache persistente automático. A opção JSON local é explícita; consumo exclusivamente estático corresponde ao Nível 2 (70% da pontuação do módulo de aquisição), enquanto o GET durante a execução atende ao Nível 1.

## Tabela Hash própria e memória

A escolha se justifica pela operação principal de consulta por identificador. Um `vector<Node*>` armazena as cabeças dos baldes. Cada `Node`, alocado com `new`, contém um corpo, o hash calculado e o ponteiro `next`. As cadeias são listas simplesmente encadeadas próprias. Não há uso de map, unordered_map ou list para substituir a estrutura.

A função FNV-1a de 64 bits percorre bytes sem sinal; o overflow de inteiro sem sinal é definido em C++. O índice é `hash % capacidade`. A função é determinística, não criptográfica e não oferece proteção contra entradas adversariais.

Inserção procura primeiro a chave na cadeia. Duplicata atualiza o corpo, retorna false, não aumenta `n` e não provoca expansão. Uma chave nova retorna true e é ligada ao início da cadeia. A chave não pode ser alterada por um ponteiro retornado, pois a consulta fornece `const CelestialBody*`.

O destrutor percorre e libera os nós iterativamente, evitando recursão profunda. A cópia é proibida para impedir dupla liberação. A movimentação transfere baldes e contadores; o objeto de origem continua destrutível e reutilizável. O nó novo fica temporariamente sob `unique_ptr` até sua ligação, protegendo a inserção contra falha de alocação do novo vetor de baldes.

Ponteiros de consulta não transferem propriedade. Permanecem válidos durante o rehashing, que religa os mesmos nós; deixam de ser válidos após destruição ou substituição do catálogo. Atualizações podem alterar o conteúdo observado. O sistema não é concorrente.

## Colisões e instrumentação

**Uma colisão de inserção é a inserção de uma chave nova em um balde que já contém pelo menos um nó, na capacidade vigente após eventual expansão.** Conta-se um evento por inserção, e não um evento por comparação na cadeia.

- Atualização de chave repetida não incrementa colisões.
- Rehashing não incrementa colisões, mesmo que nós relocalizados compartilhem balde.
- O contador é histórico: não representa quantas cadeias estão ocupadas atualmente.
- Nova carga cria nova tabela, com contador inicialmente zero.
- Métricas são atualizadas no núcleo da Hash, não inferidas externamente.

`statistics()` disponibiliza elementos, capacidade, colisões, fator de carga e rehashings. O menu mostra todas as métricas após uma carga e na opção 7. O teste controlado insere quatro IDs no mesmo balde: registra 2 colisões antes da expansão e 3 após a quarta inserção, sem contar as relocações.

## Fator de carga e rehashing

`α = n/m`, onde `n` é o número de IDs únicos e `m` a capacidade. A capacidade inicial é 16, configurável nos testes. Se a próxima inserção nova exceder 0,75, a capacidade dobra. O limite é uma escolha conservadora para manter cadeias curtas; encadeamento separado permite fatores maiores, mas não são desejados neste projeto.

O rehashing aloca um vetor de baldes vazio e religa cada nó segundo o hash armazenado e o novo módulo. Não faz buscas de duplicatas nem chama a inserção pública; assim evita contagem artificial de colisões e evita percorrer repetidamente uma cadeia durante a redistribuição. A estrutura não reduz a capacidade automaticamente.

## Complexidades

Considere `L` o comprimento de uma chave, `n` elementos e `m` baldes. Comparações textuais também dependem do tamanho das strings.

| Operação | Esperado / amortizado | Pior caso |
|---|---|---|
| Calcular hash | O(L) | O(L) |
| Buscar ID | O(L), para IDs limitados O(1) | O(L + nL) em cadeias adversariais |
| Inserir sem expansão / atualizar | O(L), com distribuição adequada | O(L + nL), além da cópia dos atributos |
| Rehashing isolado | O(m+n) | O(m+n), mesmo com todos os nós num balde |
| Inserções com crescimento | Expansão O(1) amortizada; operação O(1) esperada para chaves limitadas | Uma inserção pode disparar O(n) de expansão |
| Percorrer Hash / destruir | O(m+n) | O(m+n) |
| Pesquisa / filtros | Varredura O(m+n), mais comparação de atributos e ordenação dos resultados | O(m+n+k log k), para k resultados com IDs limitados |
| Listagem ordenada | O(m+n+n log n) | Igual |
| Planejador, dado vetor de candidatos | O(n log n) tempo, O(n) espaço | Igual |

No catálogo construído por crescimento a partir de 16, `m=O(n+1)`. Busca por atributos não se beneficia de índice por ID; ela percorre a estrutura e ordena os resultados por ID para apresentação determinística. O custo de cópia de strings é adicional às estimativas com chaves limitadas.

## Análise amortizada detalhada do rehashing

A análise trata especificamente do trabalho de expansão, separando-o do custo esperado de buscar numa cadeia. Não se afirma que encadeamento adversarial passa a ser constante por amortização.

Seja `m0` a capacidade inicial fixa e `αmax=3/4`. Ao dobrar uma capacidade `mj`, inicializamos `2mj` cabeças e percorremos `mj` baldes mais `nj ≤ mj` nós. Existe uma constante `c` tal que esse evento custa no máximo `c·mj`. O hash do ID já está armazenado; não é recalculado durante a expansão.

Para `r` expansões, as capacidades anteriores são `m0, 2m0, ..., 2^(r-1)m0`. Portanto:

```text
Trabalho de expansão ≤ c·m0·(1 + 2 + ... + 2^(r-1))
                    = c·m0·(2^r - 1)
                    < c·M,
```

onde `M=2^r m0` é a capacidade final. Se houve expansão, a última foi acionada porque a inserção faria `n > (3/4)·(M/2)`. Logo `M < (8/3)·n`, e o total de trabalho de expansão é `O(n)`. Somando `O(n)` ligações de nós e a inicialização `O(m0)`, temos `O(n+m0)`; para `m0` fixo, `O(n)` numa sequência de n inserções distintas.

Assim, o custo médio **amortizado da expansão por inserção** é O(1), embora uma inserção individual possa custar O(n). Amortizado considera a soma de uma sequência e não depende de probabilidade. Já a hipótese de distribuição adequada é necessária para obter O(1) **esperado** de busca e inserção completas, com comprimento limitado de chave.

Atualizações de duplicatas não acionam expansão. A tabela não contrai e não possui remoção pública nesta etapa; não há alternância entre crescimento e redução que invalide o argumento geométrico. Se a capacidade inicial for fornecida arbitrariamente, seu custo O(m0) deve ser contabilizado separadamente.

## Menu e operações

1. Carregar: API dinâmica ou arquivo JSON escolhido explicitamente.
2. Consultar: ID exato, por exemplo `terre`, `mars` ou `lune`.
3. Pesquisar: trecho do nome ou intervalo de gravity, meanRadius ou avgTemp.
4. Listar: todos os corpos, ordenados por ID.
5. Filtrar: tipo ou condição de planeta.
6. Comparar: consultar dois IDs na Hash, mostrar características e diferenças físicas conhecidas.
7. Estatísticas: fonte, elementos, capacidade, colisões, fator de carga e expansões.
8. Planejar: orçamento e limite de missões, candidatos excluídos, destinos e totais.
9. Encerrar.

A comparação ajuda a interpretar possíveis destinos e exercita efetivamente a Hash. A pesquisa usa normalização de maiúsculas ASCII, adequada aos nomes ingleses; não implementa normalização Unicode. Intervalos incluem seus limites e ignoram atributos ausentes. O valor false de isPlanet não é confundido com campo ausente. IDs são sensíveis a maiúsculas. Números decimais usam ponto. Entradas inválidas geram mensagem e retornam ao menu; fim de entrada encerra inclusive durante um submenu.

## Interfaces para a Parte 2

`TrieIndex` define insert/erase da associação nome–ID, consulta exata, prefixo e quantidade de nós visitados na última busca. Deve aceitar vários IDs para um mesmo nome. A implementação futura deve documentar normalização de nomes e comportamento de métricas.

`BTreeIndex` define insert/erase da associação valor–ID, consulta exata, intervalo inclusivo, altura e splits. A chave ordenada futura será `(valor, ID)` para preservar corpos com valores físicos iguais. Valores não finitos devem ser rejeitados. Altura e convenção para árvore vazia devem ser documentadas na Parte 2.

Ambas são classes abstratas com destrutor virtual, sem nós ou implementação nesta etapa. Resultados são IDs, resolvidos pela Hash do catálogo. Na Parte 2, os índices devem ser construídos e atualizados junto do catálogo; a UI continuará sem conhecer sua representação interna. Trie poderá acelerar prefixos, e Árvore B consultas físicas por intervalo.

## Planejamento guloso — Opção A

Cada destino é indivisível e pode ser selecionado uma única vez. O objetivo é maximizar a soma de benefícios sujeitos a:

```text
Σ custo_i ≤ orçamento
quantidade de destinos ≤ limite de missões
```

São duas restrições: orçamento e cardinalidade. A heurística é uma variante da mochila 0/1 com limite de quantidade; não resolve trajetórias orbitais.

### Elegibilidade e estimativas

A Terra (`terre`) é base, e estrelas ficam fora do escopo. Exigem-se categoria reconhecida, raio positivo e gravidade positiva, ambos finitos. Ausência ou zero exclui o corpo apenas do planejamento, não da consulta. Motivos de exclusão são apresentados. A política é conservadora e reduz a cobertura, evitando estimativas artificialmente baratas por falta de dados.

Definimos, para raio r em km e gravidade g em m/s²:

```text
R = log10(1 + r / 100 km)
G = g / (9,80665 m/s²)
benefício B = W + 2R
custo C = 10 + 5R + 2G
```

| Tipo | W |
|---|---:|
| Planet | 8 |
| Dwarf Planet | 10 |
| Moon | 7 |
| Asteroid | 6 |
| Comet | 9 |

As razões dentro das fórmulas são adimensionais. O logaritmo evita que o tamanho domine linearmente a escala. A categoria expressa prioridades fictícias da agência; tamanho contribui para interesse e esforço estimados, e gravidade aumenta esforço. Para entradas elegíveis, B e C são positivos. Valores não finitos derivados são rejeitados.

Esses pesos e coeficientes foram escolhidos como **modelo didático**, não como estimativa científica validada. Custos não são preços reais de missões e benefícios não medem valor científico real. Temperatura e massa estão disponíveis para consultas, mas não entram na fórmula inicial devido à incompletude dos dados e à intenção de manter o modelo explicável. Semieixo maior é parâmetro orbital, não distância real de viagem; não entra no custo.

### Critério e passos

1. Calcular benefício/custo de cada candidato.
2. Ordenar por razão decrescente, depois custo crescente e ID crescente.
3. Percorrer a ordem: selecionar se o candidato cabe no orçamento restante e ainda há uma vaga.
4. Pular candidatos que não cabem, continuando a verificar os seguintes.
5. Exibir destinos, razões, custos, benefícios, orçamento restante e vagas usadas.

Não há tolerância que autorize exceder o orçamento; utiliza-se double, sujeito ao arredondamento numérico normal. O orçamento deve ser finito e não negativo. Limite zero e orçamento zero geram plano vazio. IDs duplicados e custos inválidos são rejeitados pelo planejador. O limite de entrada do menu é 1.000.000 missões; na prática a coleção é muito menor.

### Limitação e contraexemplo executável

A escolha de maior retorno por unidade de custo pode ocupar recursos necessários para uma combinação superior. Não há garantia de ótimo global para destinos indivisíveis e restrição de quantidade.

| Destino sintético | Custo | Benefício | Razão |
|---|---:|---:|---:|
| A | 6 | 12 | 2,0 |
| B | 5 | 9 | 1,8 |
| C | 5 | 9 | 1,8 |

Com orçamento 10 e limite 2, a heurística seleciona A, sobra 4 e o benefício é 12. Selecionar B+C custa 10 e beneficia 18. O teste automatizado verifica ambas as contas. Os candidatos são sintéticos, usados para testar o algoritmo de seleção independentemente das fórmulas físicas.

## Testes e validação

A suíte possui 12 grupos internos e verifica modelo, JSON inválido, ausência, zero, tipos incorretos, HTTP simulado, autenticação, falhas de rede, inserção, busca, duplicatas, colisões controladas, fator de carga, rehashing, integridade de 1.004 elementos, transferência de propriedade, pesquisas, filtros, recarga transacional, interfaces abstratas, fórmulas, restrições, desempates, contraexemplo e menu com fluxos locais e EOF.

Testes de rede são separados da suíte determinística: com token configurado, abra `./build/missoes`, escolha 1 e depois 1. A disponibilidade externa não deve tornar o CTest instável. Confira a origem API dinâmica e as métricas após a carga.

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan -j2
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
# Alternativa, se instalado:
valgrind --leak-check=full --error-exitcode=1 ./build/cronicas_tests
```

Resultados realmente executados, limitações do ambiente e histórico estão em [docs/validacao.md](docs/validacao.md). O roteiro de avaliação e os conceitos a revisar estão em [docs/arguicao.md](docs/arguicao.md).
