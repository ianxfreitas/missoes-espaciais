# Roteiro de explicação à professora — até 10 minutos

**Objetivo:** demonstrar o sistema pelo menu e, a cada opção, mostrar o trecho de código responsável. A fala planejada dura **9 minutos**, deixando **1 minuto para espera da API, troca de arquivos ou perguntas curtas**. Ensaiar com cronômetro; não ler arquivos inteiros nem todos os resultados do terminal.

## Preparação antes da apresentação

- Deixar terminal e editor lado a lado, na raiz do projeto.
- Deixar abertos `main.cpp`, `TerminalUI.cpp`, `BodyCatalog.cpp`, `HashTable.cpp` e `MissionPlanner.cpp`. Usar a busca do editor pelo nome das funções indicadas abaixo.
- Compilar e testar previamente, sem gastar o tempo da explicação esperando a compilação:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

- Configurar `SOLAR_API_KEY` sem mostrar seu valor. Não abrir arquivos com credenciais nem imprimir a variável.
- Iniciar `./build/missoes`. Para ensaiar sem rede, usar `./build/missoes --json tests/fixtures/bodies.json`; esse modo é local, não demonstra aquisição dinâmica.
- Se a professora pedir compilação ao vivo, executar os comandos já preparados e usar o minuto de reserva. Se houver demora, encurtar as falas de pesquisa, listagem e filtros.

## Tempo e sequência

| Tempo | Ação | Código a mostrar |
|---|---|---|
| 0:00–0:30 | Introdução e fluxo geral | `main()` e `TerminalUI::run()` |
| 0:30–1:50 | 1 — Carregar dados | `load()`, `fetch()`, parser e carga do catálogo |
| 1:50–2:50 | 2 — Consultar ID | `BodyCatalog::find()` e `HashTable::find()` |
| 2:50–3:25 | 3 — Pesquisar | `searchName()` e `select()` |
| 3:25–3:45 | 4 — Listar | `list()` e `elements()` |
| 3:45–4:15 | 5 — Filtrar | `filterType()` |
| 4:15–4:45 | 6 — Comparar | `TerminalUI::compare()` |
| 4:45–6:25 | 7 — Estatísticas e mecanismo da Hash | `Node`, `insert()`, `rehash()` e métricas |
| 6:25–8:05 | 8 — Planejar missões | `estimateCandidates()` e `planMissions()` |
| 8:05–8:20 | 9 — Encerrar e memória | retorno do menu e destrutor da Hash |
| 8:20–9:00 | Testes e interfaces da Parte 2 | testes, `TrieIndex` e `BTreeIndex` |
| 9:00–10:00 | Reserva | Perguntas curtas e eventuais atrasos |

## 0:00–0:30 — Introdução e fluxo geral

**Mostrar:** [src/main.cpp](src/main.cpp), depois `run()` em [src/TerminalUI.cpp](src/TerminalUI.cpp).

**Fala sugerida:**

> “A ideia do nosso trabalho é pegar informações do Sistema Solar, organizar esses dados e ajudar a escolher quais destinos explorar. Vou mostrar isso pelo menu e, em cada opção, abrir a parte do código que faz o trabalho. Aqui no main o programa prepara tudo e abre o menu. A partir daí, cada opção chama a função responsável.”

**Fluxo para apontar no código:**

```text
main → TerminalUI::run → operação escolhida
     → catálogo / aquisição / planejador → resultado no terminal
```

## 0:30–1:50 — Opção 1: carregar dados

**Digitar:** `1`, depois `1` para API dinâmica.

**Mostrar, sem percorrer os arquivos inteiros:**

1. `TerminalUI::load()` em [src/TerminalUI.cpp](src/TerminalUI.cpp).
2. `SolarApiClient::fetch()` em [src/SolarApiClient.cpp](src/SolarApiClient.cpp).
3. Apontar `CurlHttpClient::get()` em [src/HttpClient.cpp](src/HttpClient.cpp) e `parseBodies()` em [src/BodyJsonParser.cpp](src/BodyJsonParser.cpp).
4. `BodyCatalog::load()` em [src/BodyCatalog.cpp](src/BodyCatalog.cpp).

**Fala sugerida:**

> “Aqui o programa busca os dados na API. A chave de acesso fica fora do código, numa variável do ambiente. Quando a resposta chega, conferimos se deu tudo certo e transformamos os dados em objetos que o programa consegue usar. Nem todos os corpos têm todas as informações, então tratamos o que estiver faltando. Primeiro montamos a nova tabela; só depois trocamos os dados antigos. Assim, se a carga falhar, não perdemos o que já tínhamos.”

**Apontar:** o modelo em [include/cronicas/CelestialBody.hpp](include/cronicas/CelestialBody.hpp), especialmente `id` e `std::optional`.

**Concluir:**

> “Depois de carregar, os dados ficam na memória do programa. Então a gente pode consultar e filtrar sem precisar acessar a internet toda vez.”

**Se a API falhar:** explicar a mensagem e escolher `1 → 2 → tests/fixtures/bodies.json`. Dizer explicitamente que passou a usar o conjunto local de teste. Não gastar tempo tentando resolver a rede durante a apresentação.

## 1:50–2:50 — Opção 2: consultar por identificador

**Digitar:** `2`, depois `terre`.

**Mostrar:** `find()` em [include/cronicas/BodyCatalog.hpp](include/cronicas/BodyCatalog.hpp) e [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida:**

> “Aqui vou buscar a Terra pelo ID terre. A Hash usa esse identificador para decidir em qual posição procurar. Em vez de conferir todos os corpos, ela vai até aquela posição e olha a lista que está ali. Encontrando o ID, mostra os dados; se não encontrar, avisa. Normalmente essa busca é rápida, mas pode demorar mais se muitos corpos acabarem na mesma lista.”

**Apontar:** `showBody()` só apresenta o resultado; não realiza a busca. Não repetir a consulta com ID inexistente, salvo se houver tempo ou pergunta.

## 2:50–3:25 — Opção 3: pesquisar por nome

**Digitar:** `3 → 1 → mars`.

**Mostrar:** `searchName()` e `select()` em [src/BodyCatalog.cpp](src/BodyCatalog.cpp).

**Fala sugerida:**

> “Agora vou procurar pelo nome. Como a nossa Hash foi organizada pelo ID, aqui a busca funciona de outro jeito: passamos pelos corpos e vemos quais nomes contêm o texto digitado. Não faz diferença escrever em maiúsculas ou minúsculas. Depois, organizamos os resultados pelo ID para mostrar sempre na mesma ordem.”

**Só mencionar:** o mesmo submenu permite intervalos de gravidade, raio ou temperatura por `filterRange()`, com limites inclusivos e exclusão de campos ausentes.

## 3:25–3:45 — Opção 4: listar

**Digitar:** `4`. Não ler a coleção inteira; mostrar algumas linhas e o total.

**Mostrar:** `BodyCatalog::list()` e `HashTable::elements()`.

**Fala sugerida:**

> “Essa opção mostra todos os corpos que já carregamos. O programa passa pelas posições da tabela e pelas listas de cada posição, reúne os resultados e mostra tudo em ordem de ID. Não precisa buscar os dados na API de novo.”

## 3:45–4:15 — Opção 5: filtrar

**Digitar:** `5 → 1 → Planet`.

**Mostrar:** `filterType()` em [src/BodyCatalog.cpp](src/BodyCatalog.cpp).

**Fala sugerida:**

> “Aqui eu escolho o tipo de corpo que quero ver, por exemplo, só planetas. A função passa pelos dados e deixa na lista de resultados apenas os que combinam com o filtro. Também dá para escolher pela indicação de planeta. Se essa informação estiver faltando, não vamos assumir que a resposta é não.”

## 4:15–4:45 — Opção 6: comparar dois corpos

**Digitar:** `6 → terre → mars`.

**Mostrar:** `compare()` em [src/TerminalUI.cpp](src/TerminalUI.cpp).

**Fala sugerida:**

> “Essa opção serve para colocar dois corpos lado a lado. Vou comparar a Terra com Marte. O programa busca os dois pela Hash e mostra as diferenças de gravidade, tamanho e temperatura, quando esses dados estão disponíveis. Isso ajuda a entender melhor os possíveis destinos. Esse valor de semieixo maior descreve a órbita; ele não diz quanto uma nave teria que viajar.”

## 4:45–6:25 — Opção 7: estatísticas e funcionamento interno da Hash

**Digitar:** `7`.

**Mostrar:** `Node` e os contadores em [include/cronicas/HashTable.hpp](include/cronicas/HashTable.hpp); `insert()` e `rehash()` em [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida, em três blocos:**

> “A tabela tem várias posições, e cada uma aponta para uma lista. Cada item dessa lista guarda um corpo e aponta para o próximo. Se dois IDs diferentes caem na mesma posição, colocamos os dois na lista. É assim que resolvemos as colisões. Se o mesmo ID chegar de novo, atualizamos seus dados em vez de guardar uma cópia repetida.”

> “Esses números são contados pela própria tabela. Contamos uma colisão quando um ID novo entra numa posição que já tem alguém, considerando a tabela depois de uma eventual expansão. Atualizar um ID existente ou reorganizar a tabela não aumenta esse contador. O fator de carga é a quantidade de corpos dividida pela quantidade de posições. Ele ajuda a acompanhar quanto a tabela está ocupada.”

> “Quando uma inserção faria esse fator passar de 0,75, dobramos o tamanho da tabela e reorganizamos os itens. Esse processo se chama rehashing. Os corpos continuam nos mesmos nós; mudamos as ligações para colocá-los nas posições certas. Reorganizar tudo dá trabalho, mas não acontece a cada inserção. Como a capacidade dobra, esse trabalho fica diluído ao longo das inserções. É isso que explicamos na análise amortizada: a expansão tem custo constante por inserção quando olhamos a sequência inteira. A busca ainda pode ser ruim se muitos IDs caírem juntos.”

**Não decorar números de uma carga anterior:** mostrar os contadores efetivamente produzidos naquele momento. O contador histórico de colisões não mede quantos baldes estão ocupados agora.

## 6:25–8:05 — Opção 8: planejamento guloso

**Digitar:** `8 → 100 → 3` — orçamento 100, limite de três missões.

**Mostrar:** [src/MissionPlanner.cpp](src/MissionPlanner.cpp). Ir ao fim da saída para mostrar os selecionados e os totais; não ler todos os excluídos.

**Fala sugerida:**

> “Aqui a ideia é escolher destinos sem passar do orçamento e do número de missões que a gente definiu. Vou usar orçamento 100 e até três missões. O programa dá uma pontuação de benefício e um custo estimado para cada destino, usando o tipo, o raio e a gravidade. Esses valores são inventados para o exercício, não são preços reais de viagens espaciais. A Terra é nossa base, e destinos sem dados suficientes ficam de fora dessa escolha, mas continuam disponíveis nas consultas.”

**Apontar as fórmulas em `estimateCandidates()`:**

```text
R = log10(1 + raio / 100)
G = gravidade / 9,80665
benefício = peso da categoria + 2R
custo = 10 + 5R + 2G
```

> “Depois, o programa dá prioridade a quem oferece mais benefício por unidade de custo. Ele começa pelos melhores nessa comparação e vai escolhendo enquanto houver dinheiro e vagas. Se um destino não cabe no orçamento que sobrou, pula esse e tenta o próximo. Se houver empate, escolhe o mais barato e depois usa o ID. É por isso que chamamos de guloso: ele escolhe o que parece melhor naquele momento.”

**Apontar os totais e explicar a limitação:**

> “Essa escolha respeita os limites, mas pode deixar passar uma combinação melhor. No nosso teste, A custa 6 e dá benefício 12. B e C custam 5 cada e dão benefício 9 cada. Com orçamento 10, o programa escolhe A primeiro pela melhor relação entre benefício e custo, e não consegue levar mais nenhum. Só que B junto com C daria 18. Então a estratégia é simples e rápida, mas não garante o melhor resultado possível.”

## 8:05–8:20 — Opção 9: encerrar e liberar memória

**Digitar:** `9`.

**Mostrar:** `~HashTable()` e `clear()` em [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida:**

> “Ao encerrar, a tabela libera a memória dos itens que criou. Aqui no clear, antes de apagar um item, guardamos onde está o próximo para continuar percorrendo a lista. Também impedimos a cópia da tabela, para evitar que duas tabelas tentem liberar os mesmos itens.”

## 8:20–9:00 — Testes e interfaces futuras

**Mostrar:** os nomes dos testes em [tests/tests.cpp](tests/tests.cpp), depois [TrieIndex.hpp](include/cronicas/TrieIndex.hpp) e [BTreeIndex.hpp](include/cronicas/BTreeIndex.hpp).

**Fala sugerida:**

> “Também fizemos testes para conferir buscas, colisões, atualização de IDs e crescimento da tabela, além da leitura dos dados, dos filtros e do planejamento. O exemplo em que o guloso perde para uma combinação melhor está entre esses testes. Para a próxima parte, já deixamos definidos os caminhos para usar uma Trie em buscas por nome e prefixo, e uma Árvore B em consultas por valor e intervalo. Por enquanto, só planejamos essas interfaces; as duas estruturas ainda não foram implementadas.”

Se os testes tiverem sido executados antes da apresentação, mostrar o resultado real no terminal. Não afirmar aprovação de uma execução que não ocorreu.

## Cola rápida: entradas do menu

```text
1 → 1                          carregar API
2 → terre                      consultar ID
3 → 1 → mars                   pesquisar nome
4                              listar
5 → 1 → Planet                 filtrar tipo
6 → terre → mars               comparar
7                              mostrar métricas
8 → 100 → 3                    planejar
9                              encerrar
```

## Se o tempo apertar

- Reduzir pesquisa, listagem e filtro a uma frase cada; são variações da varredura pelo catálogo.
- Priorizar Hash, instrumentação, rehashing, memória e estratégia gulosa.
- Não abrir todos os arquivos da aquisição: apontar os nomes e explicar o encadeamento.
- Reservar perguntas longas para depois da fala principal, conforme orientação da professora.
- Os dois integrantes devem compreender o fluxo completo, independentemente de quem apresenta cada trecho.
