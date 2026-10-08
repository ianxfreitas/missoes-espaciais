# Demonstração e preparação para arguição individual

Ian e Caetano devem conseguir executar e explicar todos os módulos. Esta lista é um roteiro de estudo, sem atribuição de contribuições individuais.

## Demonstração de 5 a 7 minutos

1. **Compilar e testar (até 1 minuto):** `cmake -S . -B build`, `cmake --build build -j2`, `ctest --test-dir build --output-on-failure`. Abrir `./build/missoes` com SOLAR_API_KEY já configurada sem exibir seu valor.
2. **Aquisição dinâmica (1 minuto):** opção 1, origem 1. Mostrar que dados foram carregados da API e que as estatísticas vêm da própria estrutura. Se a rede falhar, explicar o erro e carregar explicitamente o fixture com origem 2; reconhecer que essa demonstração específica será estática.
3. **Consultas (1 minuto):** opção 2, ID `terre`; opção 3, nome `mars`; opção 5, tipo `Moon`. Explicar por que o ID utiliza Hash e o nome faz varredura nesta etapa.
4. **Operação adicional (30 segundos):** opção 6, IDs `terre` e `mars`. Mostrar valores e diferenças, sem chamar semieixo maior de distância de viagem.
5. **Instrumentação (1 minuto):** opção 7. Explicar colisão como inserção nova em balde ocupado, fator n/m, limite 0,75 e crescimento por duplicação. Valores exatos podem variar com a resposta e ordem da API.
6. **Guloso (1 minuto):** opção 8, orçamento 100 e limite 3. Mostrar destinos, benefícios, custos fictícios, recursos usados e motivos de exclusão. Explicar prioridade benefício/custo e apresentar o contraexemplo A/B/C documentado no README.
7. **Memória e amortização (30 a 60 segundos):** abrir `HashTable.cpp`, mostrar nós, next, religações e destrutor; explicar soma geométrica e distinguir esperado de amortizado.

Para ensaio sem rede, executar `./build/missoes --json tests/fixtures/bodies.json`. Nesse conjunto, orçamento 100 e limite 2 permitem Mars e Moon. Nunca alegar consumo dinâmico nesse modo.

## Como os módulos funcionam

| Módulo | Mecanismo e memória | Conceito / custo |
|---|---|---|
| Modelo | Strings e optional dentro de cada corpo; massa com mantissa e expoente | Ausência difere de zero e false |
| HTTP | Handle libcurl e cabeçalhos sob RAII; resposta textual limitada | Fronteira com rede; status HTTP não é erro de parsing |
| Parser | JSON temporário convertido em vetor de corpos, avisos e rejeições | Validação de tipos; memória proporcional aos dados recebidos |
| Hash | Vetor de cabeças; nós alocados individualmente; cadeias próprias | Hash determinístico, módulo, colisão e encadeamento |
| Catálogo | Hash é dona dos corpos; resultados apontam para corpos constantes | Consulta esperada constante por ID; pesquisa linear mais ordenação |
| Interfaces futuras | Métodos virtuais puros sem nós ou armazenamento implementado | Separação entre contrato e implementação |
| Guloso | Vetor ordenado de candidatos e vetor selecionado | Escolha local sucessiva; O(n log n); sem garantia global |
| Terminal | Streams e leitura por linha; nenhuma lógica de nós na UI | Separação de responsabilidades e teste sem interação humana |

## Ponteiros e propriedade

- `Node*` contém um endereço, não uma cópia do nó.
- Cada balde aponta à cabeça de uma lista; `next` aponta ao nó seguinte ou nullptr.
- A tabela possui os nós, portanto deve fazer delete de cada um exatamente uma vez.
- Antes de delete, o destrutor guarda next. Ler head->next depois do delete seria uso de memória liberada.
- Rehashing salva next antes de alterar ligações; usa o hash armazenado e o novo módulo.
- O vector guarda ponteiros. Destruir esse vector sozinho não libera os objetos apontados.
- O novo nó fica em unique_ptr até ser ligado à cadeia, protegendo contra exceções.
- Cópia da tabela é proibida; movimento transfere propriedade e zera os contadores da origem.
- Ponteiros constantes de consulta não são donos. Uma recarga invalida os ponteiros do catálogo anterior; um rehashing preserva os endereços dos corpos.

## Perguntas prováveis e respostas a compreender

**Por que Hash?** A operação principal é consulta por ID. Uma boa distribuição e fator controlado oferecem custo esperado constante para IDs de comprimento limitado.

**Por que não unordered_map?** O trabalho exige que os mecanismos da estrutura escolhida sejam próprios. O vector apenas organiza cabeças; cadeias, hash, atualização, rehashing e contadores são implementados no projeto. O uso interno de containers pela biblioteca JSON não substitui a estrutura da aplicação.

**Duas chaves com mesmo hash são iguais?** Não. Hash igual ou índice igual somente direciona à cadeia; a comparação do ID confirma a chave.

**Como provar as colisões?** A inserção incrementa o contador exatamente quando uma chave nova chega a balde ocupado. Os testes geram IDs com o mesmo resto para capacidade conhecida e verificam o contador antes e depois da expansão.

**Por que não contar rehashing?** Ele relocaliza elementos já existentes, não recebe novas chaves. A definição usada mede eventos de inserção durante a carga. Se a docente desejar outra métrica, é preciso implementá-la e nomeá-la separadamente.

**Uma busca pode ser O(n)?** Sim, se todas as chaves estiverem numa cadeia. O limite de carga não garante distribuição uniforme. Hash determinístico não é resistência a ataques.

**Por que dobra e por que 0,75?** Dobrar permite argumento geométrico de amortização; 0,75 busca cadeias curtas com algum custo adicional de memória. Encadeamento separado poderia operar acima de 1, mas esse não é o objetivo.

**O que muda no rehashing?** O resto da divisão pela capacidade. O hash completo é o mesmo. Nós são religados, mantendo seus endereços.

**Esperado e amortizado são iguais?** Não. Esperado depende da distribuição das chaves. Amortizado considera o custo total de uma sequência: expansões geométricas somam O(n), então a expansão por inserção tem custo amortizado O(1).

**Onde está o O(n) de uma expansão?** Na inicialização dos novos baldes, percurso dos antigos e religação dos nós. Com capacidade proporcional a n, O(m+n)=O(n).

**Por que o guloso pode falhar?** A melhor razão individual pode impedir uma combinação superior. A(6,12) vence em razão, mas com orçamento 10, B(5,9)+C(5,9) beneficia 18 contra 12.

**Quais restrições?** Soma dos custos ≤ orçamento e número de destinos ≤ limite. Cada destino é indivisível e selecionado uma vez.

**Os valores são custos reais?** Não. São unidades didáticas e prioridades fictícias explicitamente documentadas. O semieixo maior não estima viagem.

**Por que excluir destinos com zero?** Não há dados físicos confiáveis suficientes para estimar esse modelo. Preservamos zero para consulta e adotamos elegibilidade conservadora apenas no planejamento.

**O que acontece se a API falhar?** Mensagem de erro, catálogo anterior preservado, sem fallback silencioso. Arquivo local pode ser escolhido explicitamente.

**O que falta na Parte 2?** Implementar Trie e Árvore B a partir dos contratos, construir seus índices associados ao catálogo e documentar suas métricas internas.

## Alterações pequenas para ensaiar

- Alterar capacidade inicial num teste e explicar quais índices mudam.
- Ajustar limite de carga, recompilar e explicar o impacto em memória e número de expansões.
- Adicionar um filtro físico seguindo o padrão de filterRange.
- Alterar orçamento/limite e prever o plano antes de executar.
- Criar um ID colidente, inserir uma duplicata e prever métricas.

Qualquer alteração deve ser seguida de compilação e testes. Conhecer os comandos não substitui entender os ponteiros e as regras de seleção.
