#include "cronicas/CelestialBody.hpp"
#include "cronicas/SolarApiClient.hpp"
#include "cronicas/HashTable.hpp"
#include "cronicas/BodyCatalog.hpp"
#include "cronicas/TrieIndex.hpp"
#include "cronicas/BTreeIndex.hpp"
#include <type_traits>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
using namespace cronicas;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    tests.push_back({"modelo", [] { CelestialBody b; b.id="terre"; check(displayName(b)=="terre", "nome alternativo"); check(!b.gravity, "ausência distinta de zero"); }});
    tests.push_back({"parsing", [] {
        auto r=parseBodies(R"({"bodies":[{"id":"x","mass":null,"gravity":0},{"id":"y","gravity":"bad"},{"englishName":"sem id"}]})");
        check(r.bodies.size()==2,"registros válidos"); check(r.bodies[0].gravity==0,"zero preservado"); check(!r.bodies[0].mass,"massa nula"); check(!r.bodies[1].gravity,"tipo inválido"); check(r.warnings.size()==2,"diagnósticos");
        for(auto text:{"{", "[]", "{}"}) { bool threw=false; try { parseBodies(text); } catch(const std::runtime_error&) { threw=true; } check(threw,"rejeição de JSON"); }
    }});
    tests.push_back({"falhas API", [] {
        struct Fake:HttpClient { long status=200; std::string text=R"({"bodies":[{"id":"x"}]})"; bool fail=false;
            HttpResponse get(const std::string&,const std::string&) const override { if(fail) throw std::runtime_error("rede simulada"); return {status,text}; }
        } fake;
        const char* previous=std::getenv("SOLAR_API_KEY"); const std::optional<std::string> saved=previous ? std::optional<std::string>(previous) : std::nullopt;
        setenv("SOLAR_API_KEY","token-de-teste",1);
        SolarApiClient api(fake); check(api.fetch().bodies.size()==1,"API simulada");
        for(long status:{401,403,429,500,302}) { fake.status=status; bool threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"status rejeitado"); }
        fake.status=200; fake.text="invalid"; bool threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"JSON API inválido");
        fake.fail=true; threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"rede");
        unsetenv("SOLAR_API_KEY"); threw=false; try { api.fetch(); } catch(const std::runtime_error&) { threw=true; } check(threw,"token ausente");
        if(saved) setenv("SOLAR_API_KEY",saved->c_str(),1);
    }});
    tests.push_back({"hash e colisões", [] {
        HashTable table(16); CelestialBody b; b.id="a"; check(table.insert(b),"inserção"); check(table.find("a"),"busca existente"); check(!table.find("missing"),"busca ausente");
        b.englishName="updated"; check(!table.insert(b),"duplicata"); check(table.size()==1 && table.collisions()==0,"duplicata não conta"); check(table.find("a")->englishName=="updated","atualização");
        std::string colliding;
        for(int i=0;i<1000;++i) { auto key="key"+std::to_string(i); if(HashTable::hashKey(key)%16==HashTable::hashKey("a")%16) { colliding=key; break; } }
        check(!colliding.empty(),"chave controlada"); b.id=colliding; table.insert(b); check(table.collisions()==1,"colisão exata"); check(table.loadFactor()==2.0/16,"fator de carga");
        check(HashTable::hashKey("a")==12638187200555641996ULL,"FNV determinístico");
    }});
    tests.push_back({"rehashing e integridade", [] {
        HashTable t(4); CelestialBody b;
        std::vector<std::string> keys;
        // Chaves no mesmo balde também após a primeira expansão.
        for(int i=0;keys.size()<4;++i) { auto k="collision"+std::to_string(i); if(HashTable::hashKey(k)%8==0) keys.push_back(k); }
        for(int i=0;i<3;++i) { b.id=keys[i]; t.insert(b); }
        check(t.capacity()==4 && t.collisions()==2,"limiar inclusivo");
        b.id=keys[3]; t.insert(b); check(t.capacity()==8 && t.rehashes()==1,"expansão"); check(t.collisions()==3,"rehash não conta colisões");
        for(int i=0;i<1000;++i) { b.id="body"+std::to_string(i); b.meanRadius=i; t.insert(b); }
        for(int i=0;i<1000;++i) { auto found=t.find("body"+std::to_string(i)); check(found && found->meanRadius==i,"integridade"); }
        auto stats=t.statistics(); check(stats.elements==1004 && stats.loadFactor<=0.75,"estatísticas");
        HashTable moved(std::move(t)); check(moved.size()==1004 && !t.find("body0"),"movimentação");
        b.id="reused"; t.insert(b); check(t.size()==1,"objeto movido reutilizável");
        t=std::move(moved); check(t.size()==1004,"atribuição por movimento");
        bool threw=false; try { HashTable invalid(0); } catch(const std::invalid_argument&) { threw=true; } check(threw,"capacidade zero");
        b.id=""; threw=false; try { t.insert(b); } catch(const std::invalid_argument&) { threw=true; } check(threw,"id vazio");
    }});
    tests.push_back({"catálogo pesquisa filtros e carga", [] {
        BodyCatalog c; check(c.list().empty(),"vazio");
        auto parsed=parseBodies(R"({"bodies":[{"id":"terre","englishName":"Earth","bodyType":"Planet","isPlanet":true,"gravity":9.8},{"id":"lune","englishName":"Moon","bodyType":"Moon","isPlanet":false},{"id":"terre","englishName":"Earth","bodyType":"Planet","isPlanet":true,"gravity":9.80665}]})");
        auto s=c.load(parsed,"teste"); check(s.inserted==2 && s.updated==1,"duplicatas na carga");
        check(c.searchName("EAR").size()==1,"pesquisa sem maiúsculas"); check(c.filterType("moon").size()==1,"tipo"); check(c.filterPlanet(true).size()==1,"planeta"); check(c.filterRange("gravity",9,10).size()==1,"intervalo e ausência");
        bool threw=false; try { c.load(ParseResult{},"inválido"); } catch(const std::runtime_error&) { threw=true; } check(threw && c.find("terre") && c.source()=="teste","carga transacional");
        for(auto field:{"gravity","bad"}) { threw=false; try { c.filterRange(field,10,9); } catch(const std::invalid_argument&) { threw=true; } check(threw,"intervalo inválido"); }
        c.load(parsed,"recarga"); check(c.statistics().elements==2,"recarga substitui");
    }});
    tests.push_back({"interfaces Parte 2", [] {
        static_assert(std::is_abstract_v<TrieIndex> && std::is_abstract_v<BTreeIndex>);
        static_assert(std::has_virtual_destructor_v<TrieIndex> && std::has_virtual_destructor_v<BTreeIndex>);
    }});
    int failures=0;
    for (const auto& t:tests) { try { t.second(); std::cout<<"PASS "<<t.first<<'\n'; } catch(const std::exception& e) { ++failures; std::cerr<<"FAIL "<<t.first<<": "<<e.what()<<'\n'; } }
    return failures ? 1 : 0;
}
