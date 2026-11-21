# Crônicas do Espaço — Planejamento Algorítmico de Missões

Projeto de Algoritmos e Estruturas de Dados II da Universidade Federal de Pelotas (UFPel). O sistema cataloga corpos celestes com dados reais do Sistema Solar, seleciona destinos sob restrições de recursos e analisa uma rede de deslocamentos, atividades, infraestrutura e transferências.

## Integrantes

- Ian Xavier Freitas.
- Caetano Seixas Blanke.

## Escopo e fonte principal

O enunciado fornecido pela disciplina é `T2 26_2 M1+M2.pdf`. Este README reúne o relatório técnico das duas partes, incluindo a análise amortizada. A Parte 1 implementou a Hash e a seleção gulosa de Planejamento e Triagem de Missões (opção A). A Parte 2 incorpora a Trie, a Árvore B, a otimização por programação dinâmica e a análise da rede espacial, com Kosaraju como funcionalidade adicional.

## Ferramentas e instalação no Fedora

C++17, CMake 3.16 ou posterior, libcurl e nlohmann-json 3.6.0 ou posterior. O mínimo do nlohmann-json corresponde à introdução de `contains()` para chaves de objeto, usada pelo parser. O CMake usa as dependências instaladas no ambiente. [Histórico de `contains()`](https://json.nlohmann.me/api/basic_json/contains/#version-history)

```bash
sudo dnf install gcc-c++ cmake libcurl-devel nlohmann-json-devel
# Opcional: diagnósticos de memória
sudo dnf install libasan libubsan valgrind
```

## Compilação, testes e execução

Na raiz do repositório, use um diretório de compilação novo. Execute os exemplos seguintes na mesma sessão Bash para manter o valor de `build_dir`. O executável é iniciado somente após a configuração, a compilação e os testes passarem.

```bash
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/cronicas-entrega.XXXXXX") &&
  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug &&
  cmake --build "$build_dir" --parallel 2 &&
  (cd "$build_dir" && ctest --output-on-failure) &&
  "$build_dir/missoes" --json tests/fixtures/bodies.json
```

Para ver cada grupo de testes, use `(cd "$build_dir" && ctest --verbose)`. O CTest registra a suíte `cronicas_tests`, que reúne os grupos internos.

```bash
"$build_dir/missoes" --help
"$build_dir/missoes" --json tests/fixtures/bodies.json
```

O arquivo de fixture contém um pequeno conjunto didático, com um corpo sintético para testar ausência de informações. Ele não representa a coleção atual inteira da API e não é apresentado como aquisição dinâmica.

## Configuração segura da API

Obtenha um token em https://api.le-systeme-solaire.net/generatekey.html. Em Bash, a entrada abaixo não ecoa o valor nem o coloca no histórico como texto literal:

```bash
read -r -s -p 'SOLAR_API_KEY: ' SOLAR_API_KEY
printf '\n'
export SOLAR_API_KEY
"$build_dir/missoes"
unset SOLAR_API_KEY
```

O programa lê a variável somente ao carregar a API. Não imprime cabeçalhos, token nem corpo bruto de erros HTTP. A opção local funciona sem chave. `.env`, chaves e diretórios de segredos estão ignorados; o programa não lê `.env` automaticamente. Nunca versionar credenciais.

## Fonte de dados e validação

API: **The Solar System OpenData**, https://api.le-systeme-solaire.net/.
Documentação oficial: https://api.le-systeme-solaire.net/ e https://api.le-systeme-solaire.net/swagger/.
Consulta documental e validação autenticada inicial em **08/10/2026**.

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
| `TrieIndex` | Interface e implementação da Trie para nomes |
| `BTreeIndex` | Interface e implementação da Árvore B para atributos físicos |
| `MissionPlanner` | Estimativas físicas, seleção gulosa e otimização por programação dinâmica |
| `MissionNetwork` | Representação e validação do cenário da rede espacial |
| `scenarios/solar-network.json` | Cenário didático com Terra, Lua e Marte |
| `scenarios/solar-network-multiple-groups.json` | Cenário didático com dois componentes fortes |
| `TerminalUI`, `main.cpp` | Menu, leitura e apresentação |
| `tests/tests.cpp` | Suíte sem dependência de rede |
| `tests/fixtures/bodies.json` | Entrada local de teste |

A libcurl é encapsulada por um contrato simples que permite substituir o transporte por um cliente simulado nos testes. Não há dependência de framework de testes.

O HTTP verifica TLS, limita a resposta a 16 MiB e utiliza timeouts de conexão de 10 s e total de 30 s. Redirecionamentos não são seguidos para evitar encaminhamento de credenciais a outro host. Erros de transporte, autenticação, limite de requisições, outros status e parsing são comunicados. Não há repetição automática nem substituição silenciosa por JSON local.

A carga constrói uma Hash temporária e usa seus registros finais para montar a Trie e as três Árvores B. Com os índices prontos, substitui o catálogo atual. Falha de aquisição, parsing, alocação ou carga vazia preserva o catálogo anterior. Campos defeituosos opcionais permitem carga parcial com avisos; não há garantia de completude quando registros são rejeitados.

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

Considere `n` corpos, `m` baldes, `k` resultados e `L` bytes na chave ou consulta. Na busca por prefixo, `s` é o número de nós visitados na subárvore e `a` é o número de associações coletadas antes de eliminar IDs repetidos. Nas varreduras, `T` é o custo total de preparar e comparar os campos de cada corpo. Os custos de ordenar e resolver IDs abaixo supõem IDs de comprimento limitado.

| Operação | Estrutura e custo de tempo |
|---|---|
| Consulta por ID | Hash. Calcular o hash custa `O(L)`. Com comprimento limitado, a consulta é `O(1)` esperada e `O(n)` no pior caso de colisões. |
| Busca por nome exato | Trie e resolução na Hash. `O(L + k)` esperado. |
| Busca por prefixo | Trie e resolução na Hash. `O(L + s + a log(a + 1))` esperado, incluindo ordenação e deduplicação dos IDs. |
| Filtro numérico por valor ou intervalo | Árvore B e resolução na Hash. O índice custa `O(log(n + 1) + k)`. A ordenação final por ID acrescenta `O(k log(k + 1))`. |
| Trecho de nome, tipo ou condição de planeta | Varredura da Hash em `O(L + m + n + T + k log(k + 1))`, incluindo a preparação da consulta e a ordenação dos resultados. |
| Listagem ordenada | Varredura da Hash e ordenação em `O(m + n + n log(n + 1))`. |
| Inserir sem expansão ou atualizar na Hash | `O(1)` esperado e `O(n)` no pior caso de colisões, além da cópia dos atributos. |
| Rehashing isolado | `O(m + n)`, inclusive quando todos os nós estão em um balde. |
| Inserções com crescimento | O custo da expansão é `O(1)` amortizado por inserção. Uma inserção pode disparar uma expansão de custo `O(n)` quando a capacidade inicial é fixa. |
| Percorrer ou destruir a Hash | `O(m + n)`. |
| Planejamento guloso, dado o vetor de candidatos | `O(n log(n + 1))` de tempo e `O(n)` de memória, usando `n` como número de candidatos. |

Com capacidade inicial fixa, `m = O(n + 1)`. Para consultas que resolvem `k` IDs na Hash, a resolução custa `O(k)` em média com distribuição adequada e pode custar `O(kn)` no pior caso de colisões. As complexidades das demais operações dos índices e dos planejadores estão nas seções correspondentes.

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
3. Pesquisar. O submenu oferece `1` para trecho do nome, `2` para intervalo de `gravity`, `meanRadius` ou `avgTemp`, `3` para nome exato e `4` para prefixo do nome.
4. Listar: todos os corpos, ordenados por ID.
5. Filtrar: tipo ou condição de planeta.
6. Comparar: consultar dois IDs na Hash, mostrar características e diferenças físicas conhecidas.
7. Estatísticas. Exibe fonte, elementos, capacidade, colisões, fator de carga e expansões da Hash e a altura e as divisões da Árvore B de cada atributo.
8. Planejamento guloso. Solicita orçamento e limite de missões e apresenta candidatos excluídos, destinos e totais.
9. Encerrar.
10. Planejamento por programação dinâmica. Solicita o mesmo orçamento e limite e mostra os planos dos dois algoritmos.
11. Rede espacial. Use `1` para carregar um cenário JSON, `2` para visualizar a rede, `3` para planejar um deslocamento, `4` para ordenar as atividades, `5` para planejar a infraestrutura, `6` para calcular a transferência máxima, `7` para analisar as rotas e `8` para encontrar os grupos com Kosaraju.

A comparação ajuda a interpretar possíveis destinos e exercita efetivamente a Hash. A pesquisa usa normalização de maiúsculas ASCII, adequada aos nomes ingleses; não implementa normalização Unicode. Intervalos incluem seus limites e ignoram atributos ausentes. O valor false de isPlanet não é confundido com campo ausente. IDs são sensíveis a maiúsculas. Números decimais usam ponto. Entradas inválidas geram mensagem e retornam ao menu; fim de entrada encerra inclusive durante um submenu.

## Trie e Árvore B

As fábricas `makeTrieIndex` e `makeBTreeIndex` permitem instanciar as implementações próprias sem expor os nós. As assinaturas virtuais projetadas na Parte 1 foram preservadas.

`makeTrieIndex` cria uma implementação de `TrieIndex`. Cada aresta representa um byte do nome e cada nó que encerra um nome guarda os IDs associados. `insert` mantém IDs distintos para um mesmo nome e `erase` remove um par nome–ID, podando os nós que ficam sem IDs e filhos. O catálogo normaliza os nomes de exibição para minúsculas e resolve na Hash os IDs devolvidos pela Trie, em ordem por ID.

A busca exata percorre o caminho do nome inteiro e devolve os IDs do nó final. A busca por prefixo percorre também a subárvore desse nó e devolve IDs únicos. O nome vazio na busca exata produz zero resultados. O prefixo vazio percorre todo o índice. A métrica exibida no menu conta a raiz e cada nó alcançado uma vez, inclusive em buscas sem resultado.

Considerando IDs de tamanho limitado, `L` bytes no nome ou prefixo, `A` IDs no nó final, `S` nós na subárvore e `K` associações coletadas, inserção e remoção custam `O(L + log(A + 1))`. Cada nó tem até 256 filhos. A busca exata custa `O(L + A)`. A busca por prefixo custa `O(L + S + K log(K + 1))`, incluindo a ordenação para deduplicar os IDs.

`makeBTreeIndex` cria uma Árvore B de grau mínimo 2, com até três chaves por nó e todas as folhas no mesmo nível. A chave `(valor, ID)` distingue corpos que têm o mesmo valor. A inserção promove a chave mediana de um nó cheio e divide suas chaves e filhos em dois nós. Na remoção, antes de descer para um filho com o mínimo de chaves, o algoritmo redistribui uma chave com um irmão ou funde os dois filhos com a chave separadora do pai. Uma chave interna pode ser substituída pelo predecessor ou sucessor. Quando a raiz perde a última chave, seu único filho assume a raiz, se houver.

O grau mínimo 2 foi escolhido para fins didáticos. Com até três chaves e quatro filhos por nó, pequenos conjuntos de teste já forçam divisões, redistribuições e fusões que podem ser acompanhadas durante a arguição. Para o tamanho do catálogo usado no projeto, essa escolha prioriza a facilidade de inspecionar as operações. Um grau maior pode reduzir a altura em catálogos maiores. Com grau fixo, a altura continua logarítmica.

Há uma árvore por atributo entre `gravity`, `meanRadius` e `avgTemp`. O catálogo indexa zeros presentes e ignora campos ausentes. Esses atributos e seus limites são não negativos na modelagem física atual. A interface da árvore aceita negativos finitos em usos que os admitam. Intervalos incluem ambos os extremos, e limites iguais disparam busca exata. A árvore retorna IDs na ordem de `(valor, ID)`. O catálogo resolve cada ID na Hash e mantém a ordenação por ID que o menu já usava.

Com grau fixo e IDs de tamanho limitado, `insert` e `erase` custam `O(log n)`. `exact` e `range` custam `O(log n + k)` para `k` associações retornadas. A consulta poda ramos fora dos limites, e o catálogo ainda ordena os resultados em `O(k log k)`. `height` percorre um caminho e custa `O(log n)`. A altura conta níveis e vale zero numa árvore vazia. `splits` é `O(1)` e inclui divisões da raiz desde a criação de cada índice. Uma recarga válida cria novos índices e reinicia a contagem.

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

Com orçamento 10 e limite 2, a heurística seleciona A, sobra 4 e o benefício é 12. A programação dinâmica seleciona B+C, gasta 10 e obtém benefício 18. Com orçamento 16 e limite 3, ambas selecionam os três destinos e obtêm benefício 30. Os candidatos são sintéticos, usados para testar a seleção independentemente das fórmulas físicas.

## Otimização de recursos por programação dinâmica

`optimizeMissions` usa os mesmos candidatos, custos e benefícios produzidos por `estimateCandidates`. Na mochila 0/1, cada destino tem uma variável binária `x_i`, um benefício `v_i` e um custo inteiro `w_i`. O orçamento convertido é `B` e o limite de missões é `K`.

```text
maximizar Σ v_i x_i
sujeito a Σ w_i x_i ≤ B, Σ x_i ≤ K, x_i ∈ {0, 1}
```

O custo `w_i` é o custo estimado em centésimos, arredondado para cima. O orçamento `B` é o orçamento em centésimos, arredondado para baixo. Valores exatamente em uma fronteira de centésimo são reconhecidos mesmo com o erro de representação de `double`. Essa conversão mantém o plano dentro do orçamento. O ótimo vale para o modelo discretizado. Por exemplo, duas missões de custo original `1.001` exigem `2.02` no modelo e podem ficar de fora com um orçamento de `2.019`. A comparação no menu usa os custos originais para o guloso e exibe os custos estimados, o benefício e o saldo de cada plano.

O estado `D_i(k,b)` guarda o maior benefício com os primeiros `i` destinos, exatamente `k` missões e custo de até `b` centésimos. A transição compara deixar o destino de fora (`D_(i-1)(k,b)`) com incluí-lo (`D_(i-1)(k-1,b-w_i) + v_i`), se houver orçamento e vagas. A linha de nenhuma missão começa em zero e os estados inviáveis começam em menos infinito. Percorrer a quantidade de missões em ordem decrescente preserva os estados necessários do conjunto anterior, evitando usar o mesmo destino duas vezes.

O maior benefício entre as quantidades de `0` a `K` determina o plano. Os empates favorecem menos missões e depois o menor custo discretizado. Com benefício, quantidade e custo iguais, a atualização mantém a solução anterior ao processar os IDs em ordem crescente. Um registro por candidato e estado permite recuperar os destinos em ordem inversa. Quando nenhuma seleção satisfaz os limites, o plano vazio usa zero recursos.

Para `n` candidatos considerados, orçamento efetivo `B` em centésimos e limite efetivo `K`, a programação dinâmica consome tempo `O(n K B)`. Os benefícios ocupam `O(K B)` e as decisões de reconstrução ocupam `O(n K B)` bytes. A validação e a ordenação por ID dos candidatos de entrada acrescentam `O(N log N)` para `N` candidatos. Antes das alocações, o algoritmo reduz a capacidade à soma dos custos relevantes, reduz `K` ao número de escolhas possíveis e verifica o limite de 64 MiB para as duas tabelas. Cenários que excedem o limite geram um erro e podem ser avaliados com orçamento ou limite de missões menor.

## Rede espacial

Os vértices usam os IDs exatos dos corpos celestes, validados no catálogo carregado. O cenário guarda esses IDs na ordem de entrada e usa suas posições como índices internos das extremidades. Isso permite ligar os registros diretamente e montar listas de adjacência a partir das arestas. Atividades têm IDs próprios e apontam para seu corpo; os pré-requisitos usam as posições das atividades.

| Lista JSON | Campos e significado |
|---|---|
| `vertices` | IDs dos corpos do catálogo |
| `routes` | `from`, `to`, `cost`. Deslocamento direcionado, com custo finito não negativo |
| `infrastructure` | `a`, `b`, `cost`. Ligação não direcionada, com custo de implantação finito não negativo |
| `transfers` | `from`, `to`, `capacity`. Canal direcionado, com capacidade inteira não negativa de até 64 bits |
| `activities` | `id`, `name`, `body`. Atividade e local no cenário |
| `dependencies` | `before`, `after`. A atividade de `before` é pré-requisito de `after` |

As seis listas são obrigatórias e podem ser vazias. Redes desconexas, vértices isolados, ciclos e autodependências são representáveis. O carregador rejeita referências fora do cenário, IDs de corpo ausentes no catálogo, IDs repetidos e relações duplicadas na mesma lista. Em infraestrutura, `a–b` e `b–a` representam a mesma ligação. Nas listas direcionadas, sentidos opostos são relações distintas. Um mesmo par pode aparecer em listas diferentes com dados próprios. Os IDs das atividades são independentes dos IDs dos corpos.

O cenário de exemplo usa Terra (`terre`), Lua (`lune`) e Marte (`mars`). Os custos de deslocamento em unidades didáticas permitem comparar um trajeto direto de custo 8 com duas etapas de custos 2 e 4. O retorno direto de Marte tem custo 9 para representar assimetria. Os custos de implantação 3, 5 e 7 oferecem alternativas de conexão. As capacidades inteiras 5, 4 e 2, em unidades de recurso por período, permitem estudar um gargalo no trecho Lua–Marte e um canal direto de capacidade menor. Esses pesos são parâmetros didáticos independentes dos atributos físicos da API.

Na raiz do repositório, execute `"$build_dir/missoes" --json tests/fixtures/bodies.json`. No menu, use `11 → 1`, informe `scenarios/solar-network.json` e depois use `11 → 2`. Se a carga do cenário falhar, o cenário anterior fica disponível. Uma recarga bem-sucedida do catálogo preserva a rede se todos os seus IDs ainda existirem. Quando algum ID falta, o menu informa que a rede foi descartada.

Para `V` corpos, `A` atividades e `E` relações totais, o cenário ocupa `O(V + A + E)`. Os mapas e conjuntos temporários usados para resolver IDs e checar duplicatas têm o mesmo limite de espaço. Percorrer a representação ou construir listas de adjacência a partir dela custa `O(V + A + E)`.

### Planejamento de deslocamentos com Dijkstra

`shortestTravelPath` resolve os IDs exatos da origem e do destino e monta uma lista de adjacência temporária que referencia somente as arestas direcionadas de `routes`. O algoritmo começa com custo zero na origem. Uma fila de prioridade escolhe o vértice com o menor custo conhecido. Para cada rota que sai dele, a soma do custo atual com o custo da aresta é comparada ao melhor valor já registrado para o vizinho. Uma melhora atualiza esse valor e grava o predecessor. A fila descarta entradas cujo custo já foi melhorado.

Como os custos são não negativos, o destino pode ser considerado definitivo quando sai da fila com seu menor custo. O percurso é reconstruído do destino para a origem pelos predecessores e invertido para exibição. Uma alternativa de mesmo custo preserva o predecessor registrado, inclusive em ciclos de custo zero. Um ID fora do cenário gera erro. Se a fila esvaziar com o destino ainda sem predecessor, o destino é inalcançável. Se origem e destino coincidirem, o percurso contém apenas esse vértice e custa zero. Os custos acumulados usam `long double`, com erro explícito se o menor custo exceder a faixa numérica.

Depois de carregar o cenário de exemplo, `11 → 2` mostra o arco direto `terre -> mars` de custo `8`. Em `11 → 3`, informe `terre` e `mars` para obter `terre -> lune -> mars`, de custo `6`.

Com `V` vértices e `R` rotas, a construção do índice de adjacência custa `O(V + R)`. Como cada melhora pode inserir uma nova entrada na fila, o tempo total é `O(V + R log(R + 1))` e o espaço auxiliar é `O(V + R)`. No cenário, existe no máximo uma rota por par ordenado de vértices, então o tempo também pode ser expresso como `O(V + R log(V + 1))`.

### Organização de atividades com Kahn

`orderActivities` usa as atividades e dependências do cenário. Para cada dependência `before -> after`, o algoritmo conta um pré-requisito pendente de `after` e registra que ela depende de `before`. A fila começa com todas as atividades que têm contagem zero. A cada atividade processada, o algoritmo reduz a contagem de suas dependentes e coloca na fila as que chegam a zero. Atividades isoladas entram na fila inicial, e ordens diferentes podem satisfazer as mesmas dependências.

Se a fila se esgotar antes de processar todas as atividades, existe um ciclo de dependências e nenhuma ordem completa é possível. Uma autodependência também bloqueia a ordem. Um cenário sem atividades tem uma ordem válida vazia e recebe uma mensagem própria no menu. Com `A` atividades e `D` dependências, a ordenação tem tempo e espaço auxiliar `O(A + D)`.

Depois de carregar o cenário de exemplo, use `11 → 4`. O menu mostra ID, nome e corpo celeste de cada atividade na sequência `prepare → relay → survey`.

### Planejamento de infraestrutura com Kruskal

`planInfrastructure` ordena as posições das ligações não direcionadas de `infrastructure` por custo crescente. Em empate, mantém a ordem do cenário. Cada ponto começa em seu próprio componente. Uma ligação entra no plano quando seus extremos pertencem a componentes diferentes. A ligação mais barata que cruza essa separação pode ser escolhida sem perder uma solução de custo mínimo. A união reduz em um o número de componentes.

O Union-Find guarda o pai de cada vértice e o tamanho de cada componente em sua raiz. Encontrar a raiz comprime o caminho, fazendo os vértices visitados apontarem diretamente para ela. A união liga a raiz do componente menor à do maior. Ligações entre extremos da mesma componente são puladas para manter a solução acíclica.

Com um componente, o menu apresenta uma árvore geradora mínima. Se restarem vários componentes ao fim das ligações disponíveis, apresenta uma floresta geradora mínima e informa que a rede está desconexa. Os vértices isolados contam como componentes de custo zero. O caso de um único vértice é uma árvore trivial de custo zero.

Com `V` vértices e `E` ligações de infraestrutura, o tempo é `O(V + E log(E + 1))`, dominado pela ordenação das ligações. As operações do Union-Find são quase constantes em custo amortizado. O espaço auxiliar é `O(V + E)` para os conjuntos, os índices ordenados e as ligações selecionadas.

Para reproduzir no cenário de exemplo, carregue `scenarios/solar-network.json` e use `11 → 5`. As duas ligações selecionadas são `terre -- lune` (3) e `lune -- mars` (5), com custo total 8.

### Transferência máxima com Edmonds–Karp

`maximumTransfer` resolve os IDs da origem e do destino e usa os canais de `transfers`. Para cada canal, a consulta monta um par de arestas residuais temporárias. A direta começa com a capacidade do canal e indica quanto fluxo ainda pode ser enviado. A reversa começa em zero e indica quanto fluxo já enviado pode ser desfeito. Canais reais em sentidos opostos têm pares independentes.

A BFS encontra um caminho aumentante com o menor número de arestas residuais. O algoritmo envia o mínimo das capacidades restantes do caminho. A atualização subtrai esse valor de cada aresta usada e soma à aresta oposta do mesmo par. Usar uma reversa reduz o fluxo do canal original, liberando capacidade para outra combinação de caminhos. O fluxo final de cada canal é a capacidade restante em sua reversa. O fluxo respeita a capacidade de cada canal e se conserva nos vértices intermediários.

Quando uma BFS deixa de alcançar o destino, os vértices ainda alcançáveis pela origem definem um lado do corte mínimo. Os canais originais que saem desse lado para o outro compõem o corte, e a soma de suas capacidades coincide com o fluxo máximo. Origens e destinos distintos sem transferência possível têm fluxo zero e corte de capacidade zero. Origem igual ao destino tem fluxo zero e resultado sem corte. O total usa `uint64_t`. Se a soma ultrapassar a faixa, a consulta sinaliza o erro antes de somar.

Para `V` vértices e `E` canais, a construção residual usa tempo e espaço `O(V + E)`. Cada BFS percorre até `O(E + 1)` elementos alcançáveis. Edmonds–Karp faz até `O(V E)` aumentos e a consulta tem tempo `O(V + E + V E²)` e memória auxiliar `O(V + E)`.

No cenário de exemplo, `11 → 6` com origem `terre` e destino `mars` apresenta fluxo máximo 6. Quatro unidades seguem pela Lua e duas pelo canal direto. O corte mínimo contém `lune → mars` (capacidade 4) e `terre → mars` (capacidade 2).

### Análise das rotas de deslocamento

`analyzeRoutes` usa somente `routes` e monta um índice temporário que referencia as rotas pelos dois extremos. A DFS percorre somente as rotas que saem do vértice atual. A pilha explícita guarda o caminho em andamento. Cada vértice pode estar inédito, ativo na pilha ou finalizado. Uma aresta para um vértice ativo fecha um ciclo. O algoritmo devolve o trecho da pilha desde esse vértice e repete seu ID no fim para mostrar o fechamento. Um laço também forma um ciclo.

Um **componente fracamente conectado** é um conjunto máximo de vértices que podem ser ligados por rotas quando se ignora o sentido. Para encontrá-los, uma busca percorre as rotas incidentes pelos dois extremos. Um **vértice isolado** tem zero rotas de entrada e zero de saída e forma seu próprio componente. Um laço conta como uma rota incidente. Infraestrutura, canais de transferência e dependências têm suas próprias operações no menu.

Um ciclo oferece um circuito de retorno compatível com o sentido das rotas, por exemplo para planejar uma missão que deve voltar à base. Componentes diferentes indicam regiões entre as quais não há deslocamento no cenário. Um vértice isolado precisa de novas rotas para participar de um itinerário. Dentro do mesmo componente fraco, uma ligação de mão única pode permitir a ida e impedir a volta. Use Dijkstra para conferir cada sentido do deslocamento.

O tempo e a memória auxiliar são `O(V + R)` para `V` vértices e `R` rotas. Use `11 → 7` depois de carregar `scenarios/solar-network.json`. O cenário mostra o ciclo `terre → lune → mars → terre`, um componente fraco e zero isolados.

### Opção A — Kosaraju

A funcionalidade adicional escolhida para a Parte 2 é a **Opção A da Seção 6**, análise de componentes fortemente conectados com Kosaraju. Cada grupo é um conjunto máximo de corpos que se alcançam mutuamente por caminhos direcionados. A análise usa somente a existência e o sentido das rotas de deslocamento. Os custos de viagem, as ligações de infraestrutura, os canais de transferência e as dependências são tratados em suas respectivas operações.

`stronglyConnectedRouteComponents` faz duas passagens de DFS com pilhas explícitas. A primeira segue as saídas das rotas originais e registra cada vértice ao finalizar sua exploração. A segunda começa pelo último vértice finalizado e percorre as rotas de entrada como saídas do grafo transposto. Cada nova DFS nessa ordem encontra um componente completo. As duas passagens referenciam as rotas existentes pelo mesmo índice temporário de incidência. Um isolado forma um grupo próprio, assim como um vértice que só tem ligações de mão única com outros grupos. Uma rede vazia retorna zero grupos.

Para `V` vértices e `R` rotas, o tempo e o espaço auxiliar são `O(V + R)`. Cada passagem visita os vértices e percorre uma quantidade linear de incidências. O resultado ajuda a identificar regiões com possibilidade de ida e volta. Para calcular o custo dos trajetos entre dois corpos do grupo, use Dijkstra em cada sentido.

Execute `"$build_dir/missoes" --json tests/fixtures/bodies.json`. Em `11 → 1`, carregue um dos arquivos abaixo. Use `11 → 8` para listar os IDs e nomes em cada grupo.

| Arquivo | Grupos esperados |
|---|---|
| `scenarios/solar-network.json` | Um grupo com Terra (`terre`), Lua (`lune`) e Marte (`mars`) |
| `scenarios/solar-network-multiple-groups.json` | Um grupo com Terra e Lua e um grupo com Marte. A rota Lua → Marte permite a ida, e o cenário só tem rotas de retorno entre Terra e Lua. |

## Testes e validação

Em **21/11/2026**, duas compilações limpas em modo Debug com GCC 16.2.1, CMake 4.3.0 e libcurl 8.21.0 passaram no CTest. Uma usou nlohmann-json 3.12.0 e a outra usou a versão mínima 3.6.0, selecionada na configuração com `-Dnlohmann_json_DIR=/caminho/para/o/diretorio/do/config`. Em cada compilação, o CTest executou uma suíte com **58 grupos aprovados**. A sessão local do menu também passou com o título atualizado.

A suíte cobre modelo, JSON, HTTP simulado, Hash, catálogo e regressões do menu. Os testes da Trie e da Árvore B verificam consultas, remoção, entradas inválidas e recarga do catálogo. A programação dinâmica é comparada com enumeração exaustiva em 308 combinações de candidatos, orçamento e limite. Os testes de rede cobrem o carregamento, caminhos inalcançáveis, ciclos de dependência, florestas de infraestrutura, reajuste de fluxo e igualdade com o corte mínimo. Os testes de Kosaraju comparam a partição obtida com a alcançabilidade mútua em todos os 512 grafos direcionados de três vértices, incluindo laços.

### Roteiro de demonstração

Use o arquivo `tests/fixtures/bodies.json` conforme as instruções de compilação. Os resultados abaixo foram conferidos numa sessão do menu.

| Opção e entrada | Resultado |
|---|---|
| `2`, ID `mars` | A Hash retorna Mars. |
| `3 → 1`, trecho `art` | A varredura encontra Earth. |
| `3 → 3`, nome `eArTh` e `3 → 4`, prefixo `Mo` | A Trie encontra Earth e Moon, respectivamente, e mostra 6 e 5 nós visitados. |
| `3 → 2`, `gravity`, limites 1 e 4, depois `7` | A Árvore B encontra Moon e Mars. O índice de gravidade mostra altura 2 e uma divisão. |
| `8` e `10`, orçamento 40 e limite 2 | Os dois planejadores selecionam Lua e Marte, com custo total de aproximadamente 35,12 e benefício de 20,61. A suíte também verifica o caso sintético em que o benefício da programação dinâmica é 18 e o do guloso é 12. |
| `11 → 1`, arquivo `scenarios/solar-network.json`, depois `11 → 2` | Carrega três vértices e mostra a rota direta Terra → Marte com custo 8. |
| `11 → 3`, origem `terre` e destino `mars` | Dijkstra encontra Terra → Lua → Marte com custo 6. |
| `11 → 4` e `11 → 5` | Kahn ordena `prepare → relay → survey`. Kruskal seleciona duas ligações com custo total 8. |
| `11 → 6`, origem `terre` e destino `mars` | Edmonds–Karp encontra fluxo 6 e corte mínimo de capacidade 6. |
| `11 → 7` e `11 → 8` | A análise mostra um ciclo, um componente fraco e nenhum isolado. Kosaraju encontra um grupo. Após carregar `scenarios/solar-network-multiple-groups.json`, Kosaraju encontra dois grupos. |

### Limitações e verificações manuais

- API autenticada. Para repetir a validação de rede, configure um token conforme a seção da API, execute `"$build_dir/missoes"` e escolha `1 → 1`. Confira a fonte API dinâmica e as métricas do catálogo. Nesta rodada a suíte usou o transporte simulado e a demonstração usou dados locais. A consulta autenticada depende de token e disponibilidade do serviço.
- Dados e planejamento. O catálogo fica em memória. Os nomes têm normalização de maiúsculas ASCII. A programação dinâmica tem limite de 64 MiB nas tabelas e garante o ótimo no modelo discretizado. Os pesos dos planejadores e das relações da rede são didáticos. Kosaraju mostra alcance mútuo no cenário fornecido, com custos de viagem avaliados separadamente por Dijkstra.
- Entrega e arguição. Confira o acesso da docente à versão de entrega do repositório. Durante a demonstração, use o roteiro acima e revise a análise amortizada, os índices, a reconstrução do plano e os tipos de relações usados por cada algoritmo.

Para verificar memória com ASan e UBSan, instale os runtimes correspondentes ao compilador e use outra compilação limpa. O runtime de ASan do GCC estava ausente no ambiente de validação.

```bash
asan_dir=$(mktemp -d "${TMPDIR:-/tmp}/cronicas-asan.XXXXXX") &&
  cmake -S . -B "$asan_dir" -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON &&
  cmake --build "$asan_dir" --parallel 2 &&
  (cd "$asan_dir" && ASAN_OPTIONS=detect_leaks=1 ctest --output-on-failure)
```

Com Valgrind instalado, uma alternativa é executar `valgrind --leak-check=full --error-exitcode=1 "$build_dir/cronicas_tests"` na compilação Debug regular.
