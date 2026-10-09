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

> “Nosso sistema organiza dados reais do Sistema Solar e seleciona destinos com recursos limitados. A Parte 1 usa uma Tabela Hash própria e um algoritmo guloso. O main cria o catálogo, o cliente HTTP, o cliente da API e o menu. O menu lê uma opção e chama o módulo responsável; ele não implementa a estrutura de dados.”

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

> “Esta opção faz um GET durante a execução. O cliente obtém o token da variável de ambiente e envia autenticação Bearer. O módulo HTTP trata transporte, TLS e timeouts; o cliente da API verifica o status. O parser converte o JSON em corpos celestes. Campos opcionais podem estar ausentes, e ausência não vira zero. O catálogo insere os corpos numa Hash temporária e só substitui os dados anteriores quando a carga termina.”

**Apontar:** o modelo em [include/cronicas/CelestialBody.hpp](include/cronicas/CelestialBody.hpp), especialmente `id` e `std::optional`.

**Concluir:**

> “Depois dessa carga, as consultas usam dados em memória e não precisam chamar a API novamente.”

**Se a API falhar:** explicar a mensagem e escolher `1 → 2 → tests/fixtures/bodies.json`. Dizer explicitamente que passou a usar o conjunto local de teste. Não gastar tempo tentando resolver a rede durante a apresentação.

## 1:50–2:50 — Opção 2: consultar por identificador

**Digitar:** `2`, depois `terre`.

**Mostrar:** `find()` em [include/cronicas/BodyCatalog.hpp](include/cronicas/BodyCatalog.hpp) e [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida:**

> “O catálogo delega esta consulta diretamente à Hash. Calculamos o hash do ID e o índice pelo resto da divisão pela capacidade. Percorremos somente a lista desse balde e comparamos os IDs. Se encontramos, devolvemos um ponteiro constante para o corpo; se não, nullptr. Esse ponteiro não é dono do objeto. A busca tem custo esperado O(1) para IDs de tamanho limitado e boa distribuição, mas pode chegar a O(n) se muitas chaves ficarem na mesma cadeia.”

**Apontar:** `showBody()` só apresenta o resultado; não realiza a busca. Não repetir a consulta com ID inexistente, salvo se houver tempo ou pergunta.

## 2:50–3:25 — Opção 3: pesquisar por nome

**Digitar:** `3 → 1 → mars`.

**Mostrar:** `searchName()` e `select()` em [src/BodyCatalog.cpp](src/BodyCatalog.cpp).

**Fala sugerida:**

> “A Hash está indexada por ID, não por nome. Nesta pesquisa percorremos os corpos, normalizamos maiúsculas e minúsculas e procuramos o trecho informado. O select mantém os resultados que satisfazem a condição e os ordena por ID. A varredura é linear no catálogo, com um custo adicional de ordenação dos resultados.”

**Só mencionar:** o mesmo submenu permite intervalos de gravidade, raio ou temperatura por `filterRange()`, com limites inclusivos e exclusão de campos ausentes.

## 3:25–3:45 — Opção 4: listar

**Digitar:** `4`. Não ler a coleção inteira; mostrar algumas linhas e o total.

**Mostrar:** `BodyCatalog::list()` e `HashTable::elements()`.

**Fala sugerida:**

> “A listagem percorre os baldes e suas cadeias. O resultado contém ponteiros constantes para os corpos já armazenados, sem copiar cada corpo. O catálogo ordena por ID antes de exibir.”

## 3:45–4:15 — Opção 5: filtrar

**Digitar:** `5 → 1 → Planet`.

**Mostrar:** `filterType()` em [src/BodyCatalog.cpp](src/BodyCatalog.cpp).

**Fala sugerida:**

> “O filtro reutiliza o select com uma condição diferente: aceitar apenas o tipo solicitado. Também podemos filtrar pela condição de planeta. Um campo isPlanet ausente não é confundido com false. As operações compartilham a mesma varredura, evitando duplicar a lógica de percorrer a estrutura.”

## 4:15–4:45 — Opção 6: comparar dois corpos

**Digitar:** `6 → terre → mars`.

**Mostrar:** `compare()` em [src/TerminalUI.cpp](src/TerminalUI.cpp).

**Fala sugerida:**

> “Esta é a operação adicional do projeto. Buscamos os dois IDs na Hash, apresentamos os atributos e calculamos diferenças de gravidade, raio e temperatura quando os dois valores existem. A comparação ajuda a interpretar destinos. O semieixo maior é um parâmetro orbital e não representa distância real de viagem.”

## 4:45–6:25 — Opção 7: estatísticas e funcionamento interno da Hash

**Digitar:** `7`.

**Mostrar:** `Node` e os contadores em [include/cronicas/HashTable.hpp](include/cronicas/HashTable.hpp); `insert()` e `rehash()` em [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida, em três blocos:**

> “Cada balde guarda um ponteiro para a cabeça de uma lista própria. Cada nó contém o corpo, seu hash e o ponteiro next. Se duas chaves diferentes chegam ao mesmo balde, ficam na mesma lista: isso é encadeamento separado. Uma duplicata atualiza o corpo, sem aumentar a quantidade.”

> “A estrutura conta uma colisão quando uma chave nova é inserida num balde já ocupado, depois de eventual expansão. Conta um evento por inserção, não por comparação. Atualizações e religações do rehashing não incrementam esse contador. O fator de carga é quantidade dividida pela capacidade. Estas métricas são calculadas dentro da própria Hash.”

> “Se a próxima inserção ultrapassar 0,75, dobramos a capacidade. O rehashing usa o hash armazenado e o novo módulo para religar os mesmos nós, sem copiar os corpos. Uma expansão custa O(n) quando a capacidade é proporcional à quantidade. Como as capacidades dobram, os custos seguem uma soma geométrica e o trabalho total de expansão é O(n) numa sequência de inserções. Portanto, o custo de expansão por inserção é O(1) amortizado. Isso não elimina o pior caso de uma cadeia mal distribuída.”

**Não decorar números de uma carga anterior:** mostrar os contadores efetivamente produzidos naquele momento. O contador histórico de colisões não mede quantos baldes estão ocupados agora.

## 6:25–8:05 — Opção 8: planejamento guloso

**Digitar:** `8 → 100 → 3` — orçamento 100, limite de três missões.

**Mostrar:** [src/MissionPlanner.cpp](src/MissionPlanner.cpp). Ir ao fim da saída para mostrar os selecionados e os totais; não ler todos os excluídos.

**Fala sugerida:**

> “O planejamento tem duas restrições: orçamento e quantidade de missões. Primeiro estimamos os candidatos usando categoria, raio e gravidade. A Terra é a base; estrelas e corpos sem dados físicos suficientes ficam fora do planejamento, mas continuam consultáveis. Os custos e benefícios são didáticos, não valores reais de missões.”

**Apontar as fórmulas em `estimateCandidates()`:**

```text
R = log10(1 + raio / 100)
G = gravidade / 9,80665
benefício = peso da categoria + 2R
custo = 10 + 5R + 2G
```

> “Ordenamos por benefício dividido pelo custo, em ordem decrescente. Empates usam menor custo e depois ID. Percorremos essa ordem e selecionamos quem cabe no orçamento restante enquanto houver vagas. Se um candidato não cabe, continuamos com os seguintes. A ordenação domina o custo, O(n log n).”

**Apontar os totais e explicar a limitação:**

> “O resultado respeita as restrições, mas não garante o ótimo. No nosso teste, A custa 6 e beneficia 12; B e C custam 5 e beneficiam 9 cada. Com orçamento 10 e limite 2, o guloso escolhe A pela melhor razão e obtém 12. B mais C obteria 18. Esse caso mostra a diferença entre a melhor escolha local e a melhor combinação global.”

## 8:05–8:20 — Opção 9: encerrar e liberar memória

**Digitar:** `9`.

**Mostrar:** `~HashTable()` e `clear()` em [src/HashTable.cpp](src/HashTable.cpp).

**Fala sugerida:**

> “Ao sair do main, o catálogo é destruído e a Hash libera todos os nós. O clear guarda o próximo endereço antes do delete e percorre as listas sem recursão. A cópia da tabela é proibida para evitar dupla liberação; a movimentação transfere a propriedade.”

## 8:20–9:00 — Testes e interfaces futuras

**Mostrar:** os nomes dos testes em [tests/tests.cpp](tests/tests.cpp), depois [TrieIndex.hpp](include/cronicas/TrieIndex.hpp) e [BTreeIndex.hpp](include/cronicas/BTreeIndex.hpp).

**Fala sugerida:**

> “Os testes cobrem inserção, busca, duplicatas, colisões controladas, rehashing e integridade dos dados, além de parsing, falhas de API simuladas, filtros e planejamento. Temos um teste específico do contraexemplo guloso. Para a Parte 2, projetamos contratos abstratos: Trie para nomes e prefixos, e Árvore B para consultas ordenadas e intervalos. Elas ainda não estão implementadas. Seus resultados serão IDs que o catálogo resolve na Hash.”

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
