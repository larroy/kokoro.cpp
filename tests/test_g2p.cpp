// G2P regression goldens. They pin the current output (including linguistically imperfect
// cases); update a golden only when G2P behavior is changed deliberately.
// usage: test_g2p <dict_dir> [doctest options]
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "chinese_english/JiebaProcessor.h"
#include "NumberNormalizer.h"
#include "spanish/SpanishG2P.h"
#include "Phonemizer.h"
#include "english/EnG2P.h"
#include "english/EnglishPhonemizer.h"
#include "chinese_english/ZHG2P.h"
#include "german/GermanG2P.h"
#include "german/GermanLexicon.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::shared_ptr<JiebaProcessor> g_proc;
std::unique_ptr<ZHG2P> g_g2p;
std::unique_ptr<EnglishPhonemizer> g_en;
std::unique_ptr<GermanLexicon> g_de;

struct Case {
    const char* input;
    const char* expected;
};

template <size_t N>
void check_g2p(const Case (&cases)[N]) {
    for (const Case& c : cases) {
        const std::string input = c.input;  // doctest logs a captured const char* as a bool
        CAPTURE(input);
        CHECK((*g_g2p)(input).first == std::string(c.expected));
    }
}

}  // namespace

TEST_CASE("number_conversion") {
    static const Case cases[] = {
        {"123", "一百二十三"},
        {"-5", "负五"},
        {"3.14", "三点一四"},
        {"123.456", "一百二十三点四五六"},
        {"这里有500个苹果", "这里有五百个苹果"},
        {"气温是-3.5度", "气温是负三点五度"},
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, NumberLanguage::Auto) == std::string(c.expected));
    }
}

TEST_CASE("number_conversion_english") {
    struct NumCase {
        const char* input;
        const char* expected;
    };
    static const NumCase cases[] = {
        {"0", "zero"},
        {"12", "twelve"},
        {"25", "twenty five"},
        {"105", "one hundred five"},
        {"2024", "two thousand twenty four"},
        {"1200007", "one million two hundred thousand seven"},
        {"007", "seven"},
        {"-5", "minus five"},
        {"3.14", "three point one four"},
        {"192.168.0.1", "one nine two dot one six eight dot zero dot one"},
        {"1234567890123456",
         "one two three four five six seven eight nine zero one two three four five six"},
        {"我有3个", "我有three个"},
    };
    for (const NumCase& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, NumberLanguage::English) == std::string(c.expected));
    }
}

TEST_CASE("number_conversion_spanish") {
    static const Case cases[] = {
        {"0", "cero"},
        {"21", "veintiuno"},
        {"22", "veintidós"},
        {"31", "treinta y uno"},
        {"100", "cien"},
        {"101", "ciento uno"},
        {"500", "quinientos"},
        {"1984", "mil novecientos ochenta y cuatro"},
        {"21000", "veintiún mil"},
        {"101000", "ciento un mil"},
        {"1000000", "un millón"},
        {"2500000", "dos millones quinientos mil"},
        {"31000000", "treinta y un millones"},
        {"-5", "menos cinco"},
        {"3.14", "tres punto uno cuatro"},
        {"1234567890123", "uno dos tres cuatro cinco seis siete ocho nueve cero uno dos tres"},
        {"tengo 3 gatos", "tengo tres gatos"},
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, NumberLanguage::Spanish) == std::string(c.expected));
    }
}

TEST_CASE("number_conversion_german") {
    static const Case cases[] = {
        {"0", "null"},
        {"1", "eins"},
        {"16", "sechzehn"},
        {"17", "siebzehn"},
        {"21", "einundzwanzig"},
        {"30", "dreißig"},
        {"99", "neunundneunzig"},
        {"100", "einhundert"},
        {"101", "einhundert eins"},
        {"250", "zweihundert fünfzig"},
        {"1000", "eintausend"},
        {"1984", "eintausend neunhundert vierundachtzig"},
        {"2024", "zwei tausend vierundzwanzig"},
        {"101000", "einhundert ein tausend"},
        {"1000000", "eine million"},
        {"2500000", "zwei millionen fünfhundert tausend"},
        {"3000000000", "drei milliarden"},
        {"-5", "minus fünf"},
        {"3.14", "drei komma eins vier"},
        {"1.2.3", "eins punkt zwei punkt drei"},
        {"1234567890123", "eins zwei drei vier fünf sechs sieben acht neun null eins zwei drei"},
        {"ich habe 3 katzen", "ich habe drei katzen"},
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, NumberLanguage::German) == std::string(c.expected));
    }
}

TEST_CASE("number_language_auto_and_forced") {
    struct NumCase {
        const char* input;
        NumberLanguage language;
        const char* expected;
    };
    static const NumCase cases[] = {
        {"I have 3 apples", NumberLanguage::Auto, "I have three apples"},
        {"3 apples", NumberLanguage::Auto, "three apples"},
        {"我有3个 apples", NumberLanguage::Auto, "我有三个 apples"},
        {"IPV4地址是192.168.0.1", NumberLanguage::Auto, "IPV four地址是一九二点一六八点零点一"},
        {"I have 3 apples", NumberLanguage::Chinese, "I have 三 apples"},
        {"I have 3 apples", NumberLanguage::Spanish, "I have tres apples"},
        {"I have 3 apples", NumberLanguage::German, "I have drei apples"},
        {"tengo 3 gatos", NumberLanguage::Auto, "tengo three gatos"},
    };
    for (const NumCase& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, c.language) == std::string(c.expected));
    }
}

TEST_CASE("percent_sign") {
    struct PercentCase {
        const char* input;
        NumberLanguage language;
        const char* expected;
    };
    static const PercentCase cases[] = {
        {"35 %", NumberLanguage::English, "thirty five percent"},
        {"35%", NumberLanguage::English, "thirty five percent"},
        {"35\xEF\xBC\x85", NumberLanguage::English, "thirty five percent"},
        {"3.5%", NumberLanguage::English, "three point five percent"},
        {"It rose 35 % last year.", NumberLanguage::English, "It rose thirty five percent last year."},
        {"35% off", NumberLanguage::English, "thirty five percent off"},
        {"35%off", NumberLanguage::English, "thirty five percent off"},
        {"35 apples", NumberLanguage::English, "thirty five apples"},
        {"35%", NumberLanguage::Spanish, "treinta y cinco por ciento"},
        {"35 %", NumberLanguage::German, "fünfunddreißig prozent"},
        {"1%", NumberLanguage::German, "ein prozent"},
        {"-1%", NumberLanguage::German, "minus ein prozent"},
        {"101%", NumberLanguage::German, "einhundert ein prozent"},
        {"1.5%", NumberLanguage::German, "eins komma fünf prozent"},
        {"35%", NumberLanguage::Chinese, "百分之三十五"},
        {"35\xEF\xBC\x85", NumberLanguage::Chinese, "百分之三十五"},
        {"用了35%的电", NumberLanguage::Auto, "用了百分之三十五的电"},
        {"I have 35%.", NumberLanguage::Auto, "I have thirty five percent."},
    };
    for (const PercentCase& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, c.language) == std::string(c.expected));
    }
}

TEST_CASE("clock_time") {
    struct TimeCase {
        const char* input;
        NumberLanguage language;
        const char* expected;
    };
    static const TimeCase cases[] = {
        {"10:49 AM", NumberLanguage::English, "ten forty nine ay em"},
        {"10:49am", NumberLanguage::English, "ten forty nine ay em"},
        {"10:49 a.m.", NumberLanguage::English, "ten forty nine ay em."},
        {"7:05 PM", NumberLanguage::English, "seven oh five pee em"},
        {"10:00 AM", NumberLanguage::English, "ten ay em"},
        {"10:00", NumberLanguage::English, "ten o'clock"},
        {"12:30", NumberLanguage::English, "twelve thirty"},
        {"19:29", NumberLanguage::English, "nineteen hours twenty nine minutes"},
        {"19:00", NumberLanguage::English, "nineteen hours"},
        {"19:01", NumberLanguage::English, "nineteen hours one minute"},
        {"07:30", NumberLanguage::English, "seven hours thirty minutes"},
        {"0:30", NumberLanguage::English, "zero hours thirty minutes"},
        {"10:49:30", NumberLanguage::English, "ten hours forty nine minutes thirty seconds"},
        {"10:49:01 PM", NumberLanguage::English, "ten hours forty nine minutes one second pee em"},
        {"19:29 PM", NumberLanguage::English, "nineteen hours twenty nine minutes PM"},
        {"9:00-10:30", NumberLanguage::English, "nine o'clock-ten thirty"},
        {"3:16", NumberLanguage::English, "three sixteen"},
        {"25:30", NumberLanguage::English, "twenty five:thirty"},
        {"10:5", NumberLanguage::English, "ten:five"},
        {"10:30 amazing", NumberLanguage::English, "ten thirty amazing"},
        {"5 AM", NumberLanguage::English, "five ay em"},
        {"5pm", NumberLanguage::English, "five pee em"},
        {"5 a.m.", NumberLanguage::English, "five ay em."},
        {"12 am", NumberLanguage::English, "twelve ay em"},
        {"05 PM", NumberLanguage::English, "five pee em"},
        {"15 AM", NumberLanguage::English, "fifteen AM"},
        {"5 amber", NumberLanguage::English, "five amber"},
        {"5 a. m.", NumberLanguage::Spanish, "cinco en punto a eme."},
        {"5 PM", NumberLanguage::German, "siebzehn uhr"},
        {"5 AM", NumberLanguage::Chinese, "上午五点"},
        {"10:49", NumberLanguage::Spanish, "diez y cuarenta y nueve"},
        {"19:29", NumberLanguage::Spanish, "diecinueve y veintinueve"},
        {"1:05", NumberLanguage::Spanish, "una y cinco"},
        {"21:21", NumberLanguage::Spanish, "veintiuna y veintiuno"},
        {"19:00", NumberLanguage::Spanish, "diecinueve horas"},
        {"1:00", NumberLanguage::Spanish, "una en punto"},
        {"01:00", NumberLanguage::Spanish, "una hora"},
        {"10:49 a. m.", NumberLanguage::Spanish, "diez y cuarenta y nueve a eme."},
        {"7:30 p.m.", NumberLanguage::Spanish, "siete y treinta pe eme."},
        {"10:49:30", NumberLanguage::Spanish, "diez y cuarenta y nueve con treinta segundos"},
        {"10:49:21", NumberLanguage::Spanish, "diez y cuarenta y nueve con veintiún segundos"},
        {"10:49:01", NumberLanguage::Spanish, "diez y cuarenta y nueve con un segundo"},
        {"10:49", NumberLanguage::German, "zehn uhr neunundvierzig"},
        {"19:00", NumberLanguage::German, "neunzehn uhr"},
        {"1:00", NumberLanguage::German, "ein uhr"},
        {"10:01", NumberLanguage::German, "zehn uhr eins"},
        {"0:30", NumberLanguage::German, "null uhr dreißig"},
        {"19:29 Uhr", NumberLanguage::German, "neunzehn uhr neunundzwanzig"},
        {"Um 19:00 Uhr.", NumberLanguage::German, "Um neunzehn uhr."},
        {"10:49 PM", NumberLanguage::German, "zweiundzwanzig uhr neunundvierzig"},
        {"12:15 AM", NumberLanguage::German, "null uhr fünfzehn"},
        {"12:15 PM", NumberLanguage::German, "zwölf uhr fünfzehn"},
        {"10:49:30", NumberLanguage::German, "zehn uhr neunundvierzig und dreißig sekunden"},
        {"10:49:01", NumberLanguage::German, "zehn uhr neunundvierzig und eine sekunde"},
        {"10:49", NumberLanguage::Chinese, "十点四十九分"},
        {"19:00", NumberLanguage::Chinese, "十九点"},
        {"2:00", NumberLanguage::Chinese, "两点"},
        {"10:05", NumberLanguage::Chinese, "十点零五分"},
        {"12:30", NumberLanguage::Chinese, "十二点三十分"},
        {"0:30", NumberLanguage::Chinese, "零点三十分"},
        {"10:49 AM", NumberLanguage::Chinese, "上午十点四十九分"},
        {"10:49 PM", NumberLanguage::Chinese, "下午十点四十九分"},
        {"10:49:30", NumberLanguage::Chinese, "十点四十九分三十秒"},
        {"10:00:30", NumberLanguage::Chinese, "十点零分三十秒"},
        {"10\xEF\xBC\x9A" "49", NumberLanguage::Chinese, "十点四十九分"},  // full-width colon
        {"会议在10:30开始", NumberLanguage::Auto, "会议在十点三十分开始"},
        {"The meeting is at 19:00.", NumberLanguage::Auto, "The meeting is at nineteen hours."},
        {"Meet at 10:49 AM.", NumberLanguage::Auto, "Meet at ten forty nine ay em."},
    };
    for (const TimeCase& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(normalize_numbers(input, c.language) == std::string(c.expected));
    }
}

TEST_CASE("clock_time_phonemes") {
    // AM is spelled out (/ˈA ˈɛm/), never read as the word "am".
    const std::string am = g_en->phonemize("10:49 AM", NumberLanguage::Auto);
    CHECK(am == g_en->phonemize("ten forty nine ay em", NumberLanguage::Auto));
    CHECK(am.find("ˈA ˈɛm") != std::string::npos);
    CHECK(g_en->phonemize("It is 5 AM now", NumberLanguage::Auto) ==
          g_en->phonemize("It is five ay em now", NumberLanguage::Auto));
    CHECK(german_to_phonemes("Um 19:00 Uhr.", NumberLanguage::Auto, *g_de) ==
          german_to_phonemes("Um neunzehn Uhr.", NumberLanguage::Auto, *g_de));
}

TEST_CASE("spanish_g2p") {
    // Pinned after eval_bench/compare_g2p.py matched espeak-ng on eval_bench/corpus/es.txt.
    static const Case cases[] = {
        {"Hola mundo", "ˈola mˈundo"},
        {"Hola, ¿cómo estás?", "ˈola, ¿kˈomo estˈas?"},
        {"Buenos días.", "bwˈenos ðˈias."},
        {"Muchas gracias.", "mˈuʧas ɣɾˈaθjas."},
        {"El niño come manzanas.", "el nˈiɲo kˈome manθˈanas."},
        {"La ciudad de Madrid.", "la θjuðˈad ðe maðɾˈid."},
        {"Zapato, cereza y cebolla.", "θapˈato, θeɾˈeθa i θeβˈoʎa."},
        {"El perro corre rápido por la carretera.", "el pˈero kˈore rˈapiðo poɾ la karetˈeɾa."},
        {"Honra, alrededor, Israel.", "ˈonra, alreðeðˈoɾ, israˈel."},
        {"Guerra, guitarra, pingüino, agua.", "ɡˈera, ɡitˈara, piŋɡuˈino, ˈaɣwa."},
        {"Gente y jirafa.", "xˈɛnte i xiɾˈafa."},
        {"Queso, quince, kilo.", "kˈeso, kˈinθe, kˈilo."},
        {"Examen, taxi, xilófono.", "eksˈamen, tˈaksi, silˈofono."},
        {"Yo voy a la playa.", "ʝˈo βˈoɪ a la plˈaʝa."},
        {"Llueve en la calle.", "ʎuˈeβe en la kˈaʎe."},
        {"Bebé, vaca, ambos, hablar.", "beβˈe, bˈaka, ˈambos, aβlˈaɾ."},
        {"Cuidado con el diente.", "kwiðˈaðo kon el ðjˈɛnte."},
        {"Aire, causa, reina, oigo.", "ˈIɾe, kˈWsa, rˈAna, ˈoɪɣo."},
        {"Poeta, león, país, baúl.", "poˈeta, leˈon, paˈis, baˈul."},
        {"Canción, árbol, lápiz, reloj.", "kanθjˈon, ˈaɾβol, lˈapiθ, relˈox."},
        {"Hablar, comer, vivir, ciudad.", "aβlˈaɾ, komˈeɾ, biβˈiɾ, θjuðˈad."},
        {"Tengo un banco en Cuenca.", "tˈɛŋɡo ˈum bˈanko en kwˈɛnka."},
        {"Él tiene más de un coche.", "ˈel tjˈene mˈas ðe ˈun kˈoʧe."},
        {"Hoy es lunes.", "ˈoɪ ˈes lˈunes."},
        {"Pequeño, mañana, España.", "pekˈeɲo, maɲˈana, espˈaɲa."},
        {"¡Qué bueno!", "¡kˈe βwˈeno!"},
        {"Señor García, ¿dónde está el baño?", "seɲˈoɾ ɣaɾθˈia, ¿dˈonde estˈa el βˈaɲo?"},
        {"El agua del río está fría.", "el ˈaɣwa ðel rˈio estˈa fɾˈia."},
        {"Tengo 3 gatos y 25 perros.", "tˈɛŋɡo tɾˈes ɣˈatos i βAntiθˈinko pˈeros."},
        {"Nací en 1984.", "naθˈi en mˈil noβeθjˈɛntos oʧˈɛnta i kwˈatɾo."},
        {"Uso CPP y GPU.", "ˈuso θepepˈe i xepeˈu."},
        {"La ONU y la OTAN.", "la ˈonu i la ˈotan."},
        {"Un DVD y un PC.", "ˈun deuβeðˈe i ˈum peθˈe."},
        {"La FIFA y la NASA.", "la fˈifa i la nˈasa."},
        {"El BBVA y la RAE.", "el βeβeuβeˈa i la rˈae."},
        {"HOLA MUNDO, CHILE, ESPAÑA.", "ˈola mˈundo, ʧˈile, espˈaɲa."},
        {"El ADN de IBM.", "el aðeˈene ðe iβeˈeme."},
        {"EL ÁLBUM DEL CLUB, UN ROBOT Y LA PSICOLOGÍA EN PERSPECTIVA.",
         "el ˈalβum del klˈub, ˈun roβˈot i la sikoloxˈia em peɾspektˈiβa."},
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(spanish_to_phonemes(input, NumberLanguage::Auto) == std::string(c.expected));
    }
}

TEST_CASE("spanish_loanwords") {
    static const Case cases[] = {
        {"Voy al parking a comprar pizza y whisky.", "bˈoɪ al pˈaɾkin a kompɾˈaɾ pˈitsa i wˈiski."},
        {"El marketing del show.", "el mˈaɾketin del ʃˈO."},
        {"Champú, shampoo, shopping, software.", "ʧampˈu, ʃampˈu, ʃˈopin, sˈoftweɾ."},
        {"Me gusta el jazz y el croissant.", "me ɣˈusta el ʝˈas i el kɾwasˈan."},
        {"Dos parkings, JAZZ, whiskey.", "dˈos pˈaɾkins, ʝˈas, wˈiski."},
        {"Un güisqui en el show.", "ˈuŋ ɡwˈiski en el ʃˈO."},
        {"Lo vi en Facebook.", "lo βˈi en fˈAsβuk."},
        {"Búscalo en Google.", "bˈuskalo eŋ ɡˈuɣel."},
        {"Vi el thriller en Netflix con mi iPhone.", "bˈi el tɾˈileɾ en nˈetfliks kon mi ˈIfon."},
        {"El manager del ranking usa WhatsApp y email.", "el mˈanaʝeɾ ðel rˈankin ˈusa wˈatsap i ˈimAl."},
        {"Un sandwich, un spray y un blazer.", "ˈun sˈandwiʧ, ˈun espɾˈA i ˈum blˈAseɾ."},
        {"Spaghetti, gnocchi, lasagna y mozzarella.", "espaɣˈeti, ɲˈoki, lasˈaɲa i motsaɾˈela."},
        {"Un cappuccino y bruschetta.", "ˈun kapuʧˈino i βɾuskˈeta."},
        {"Paparazzi, graffiti, tsunami, spaghettis.", "papaɾˈatsi, ɡɾafˈiti, tsunˈami, espaɣˈetis."},
        {"Sushi, sashimi y pretzel en un yacht.", "sˈuʃi, saʃˈimi i pɾˈetsel en ˈun ʝˈot."},
        {"Un déjà vu en el kindergarten.", "ˈun dˈeʝa βˈu en el kˈindeɾɣaɾten."},
        {"Menú À LA CARTE, status quo.", "menˈu a la kˈaɾt, estˈatus kwˈo."},
        {"Mi résumé y curriculum vitae.", "mi resumˈe i kurˈikulum bˈitae."},
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(spanish_to_phonemes(input, NumberLanguage::Auto) == std::string(c.expected));
    }
}

TEST_CASE("spanish_loanwords_tsv") {
    const std::filesystem::path tsv = std::filesystem::temp_directory_path() / "kokoro_test_es_loanwords.tsv";
    {
        std::ofstream out(tsv);
        out << "# word\trespelling\n\nPizza\tpíza\nwifi\tuáifai\nmalformed\n";
    }
    const SpanishLoanwords loanwords = SpanishLoanwords::load(tsv.string());
    std::filesystem::remove(tsv);
    CHECK(spanish_to_phonemes("Pizza con wifi y jazz.", NumberLanguage::Auto, loanwords) ==
          std::string("pˈiθa kon wˈIfI i ʝˈas."));
    CHECK(SpanishLoanwords::load("/nonexistent/es_loanwords.tsv").respell(u"jazz") == std::u16string(u"yas"));
}

TEST_CASE("single_letter_words") {
    static const Case cases[] = {
        {"I am here. You and I.", "ˈI ˈæm hˈiɹ. jˈu ənd ˈI."},
        {"saw a cat.", "sˈɔ ə kˈæt."},
    };
    check_g2p(cases);
}

TEST_CASE("english_apostrophes") {
    static const Case cases[] = {
        {"What's the weather today?", "wˈʌts ðə wˈɛðəɹ tədˈA?"},  // CMU contraction
        {"I don't know.", "ˈI dˈOnt nˈO."},
        {"It’s mine.", "ˈɪts mˈIn."},                              // curly apostrophe
        {"rock'n'roll", "ɹˈɑkənɹˈOl"},                             // chained apostrophes
        {"Kokoro's", "kˈOkOɹOz"},                                  // OOV possessive: z after vowel
        {"laptop's", "lˈæptˌɑps"},                                 // s after voiceless
        {"onnx's", "ˈɑnɪksɪz"},                                    // ɪz after sibilant
        {"GPU's", "ʤˈipˈijˈuz"},                                   // acronym stem
    };
    check_g2p(cases);
}

TEST_CASE("mixed_language") {
    static const Case cases[] = {
        {"中国", "ʈʂʊ→ŋkwo↗"},
        {"你好中国", "ni↗xau̯↓ʈʂʊ→ŋkwo↗"},
        {"Hello中国!", "həlˈO ʈʂʊ→ŋkwo↗!"},
    };
    check_g2p(cases);
}

TEST_CASE("polyphones") {
    static const Case cases[] = {
        {"这个东西很便宜", "ʈʂɤ↘kɤtʊ→ŋɕixə↓npʰjɛ↗ni"},
        {"我要去长安", "wo↓jau̯↘ʨʰu↘ʈʂʰa↗ŋa→n"},
        {"着火了", "ʈʂau̯↗xwo↓lɤ"},  // zhao2
        {"看着", "kʰa↘nʈʂɤ"},       // zhe
        {"音乐", "i→nɥe↘"},         // yue4
        {"快乐", "kʰwai̯↘lɤ↘"},      // le4
    };
    check_g2p(cases);
}

TEST_CASE("numbers_in_text") {
    static const Case cases[] = {
        {"今天天气真不错", "ʨi→ntʰjɛ→ntʰjɛ→nʨʰi↘ʈʂə→npuʦʰwo↘"},
        {"我有123块钱", "wo↓jou̯↓i↘pai̯↓ɚ↘ʂɨ↗sa→nkʰwai̯↘ʨʰjɛ↗n"},
        {"今天气温-5度", "ʨi→ntʰjɛ→nʨʰi↘wə→nfu↘u↓tu↘"},
        {"圆周率是3.14159", "ɥɛ↗nʈʂou̯→ly↘ʂɨ↘sa→ntjɛ↓ni→sɨ↘i→u↗ʨiu"},
        {"IPV4地址是192.168.0.1", "ˈIpˈivˈi fˈɔɹ ti↘ʈʂɨ↓ʂɨ↘i→ʨiuɚ↘tjɛ↓ni→liupa→tjɛ↓nli↗ŋtjɛ↓ni→"},
        {"I have 3 apples and 25 pears in 2024.",
         "ˈI hˈæv θɹˈi ˈæpəlz ənd twˈɛnti fˈIv pˈɛɹz ɪn tˈu θˈWzənd twˈɛnti fˈɔɹ."},
    };
    check_g2p(cases);
}

TEST_CASE("third_tone_sandhi") {
    static const Case cases[] = {
        {"洗澡", "ɕi↗ʦau̯↓"},          // xi3 zao3 -> xi2 zao3
        {"管理", "kwa↗nli↓"},         // guan3 li3 -> guan2 li3
        {"展览馆", "ʈʂa↗nla↗nkwa↓n"},  // zhan3 lan3 guan3 -> zhan2 lan2 guan3 (333 -> 223)
    };
    check_g2p(cases);
}

TEST_CASE("yi_sandhi") {
    static const Case cases[] = {
        {"一", "i→"},             // yi1 (alone)
        {"第一", "ti↘i→"},        // yi1 (ordinal)
        {"一天", "i↘tʰjɛ→n"},     // yi4 (before tone 1)
        {"一年", "i↘njɛ↗n"},      // yi4 (before tone 2)
        {"一起", "i↘ʨʰi↓"},       // yi4 (before tone 3)
        {"一个", "i↗kɤ↘"},        // yi2 (before tone 4)
    };
    check_g2p(cases);
}

TEST_CASE("bu_sandhi") {
    static const Case cases[] = {
        {"不吃", "pu↘ʈʂʰɨ→"},   // bu4 (before tone 1)
        {"不玩", "pu↘wa↗n"},    // bu4 (before tone 2)
        {"不懂", "pu↘tʊ↓ŋ"},    // bu4 (before tone 3)
        {"不对", "pu↗twei̯↘"},   // bu2 (before tone 4)
    };
    check_g2p(cases);
}

TEST_CASE("erhua") {
    static const Case cases[] = {
        {"花儿", "xwa→ɚ↗"},       // hua er -> huar
        {"小院儿", "ɕjau̯↓ɥɛ↘nɚ"},  // yuan er -> yuanr
        {"玩儿", "wa↗nɚ"},        // wan er -> wanr
    };
    check_g2p(cases);
}

TEST_CASE("neutral_tone") {
    static const Case cases[] = {
        {"爸爸", "pa↘pa"},
        {"妈妈", "ma→ma"},
        {"看看", "kʰa↘nkʰan"},
    };
    check_g2p(cases);
}

TEST_CASE("english_phonemizer") {
    static const Case cases[] = {
        {"I am here. You and I.", "ˈI ˈæm hˈiɹ. jˈu ənd ˈI."},
        {"What's the weather today?", "wˈʌts ðə wˈɛðəɹ tədˈA?"},
        {"It’s mine.", "ˈɪts mˈIn."},
        {"rock'n'roll", "ɹˈɑkənɹˈOl"},
        {"GPU's", "ʤˈipˈijˈuz"},
        {"I have 3 apples and 25 pears in 2024.",
         "ˈI hˈæv θɹˈi ˈæpəlz ənd twˈɛnti fˈIv pˈɛɹz ɪn tˈu θˈWzənd twˈɛnti fˈɔɹ."},
        {"3", "θɹˈi"},  // Auto reads numbers as English; ZHG2P reads a lone "3" as Chinese
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(g_en->phonemize(input, NumberLanguage::Auto) == std::string(c.expected));
    }
}

TEST_CASE("german_g2p") {
    // MFA dictionary phones mapped to espeak-ng `de` conventions (src/german/GermanPronunciation.cpp).
    static const Case cases[] = {
        {"Guten Tag, dies ist ein Test.", "ɡˈuːtən tˈak, diːs ɪst In tˈɛst."},
        {"Der Hund läuft schnell durch den Garten.", "dɛɾ hˈʊnt lˈɔøft ʃnˈɛl dʊɐç deːn ɡˈaɾtən."},
        {"Er versteht die Frage nicht.", "ɛɾ fɛɾʃtˈeːt diː frˈɑːɡə nˈɪçt."},
        {"Die Studenten studieren Philosophie.", "diː ʃtʊdˈɛntən ʃtʊdˈiːrən fɪlɔzɔfˈiː."},
        {"Die Polizei sucht nach dem Fahrrad.", "diː pɔlɪʦˈI zˈʊxt nɑːx deːm fˈarat."},
        {"Ich habe 250 Euro.", "ɪç hɑːbə ʦvˈIhʊndɜt fˈynfʦɪk ˈɔørɔ."},
        {"Die Fahrradschlosskette ist kaputt.", "diː fˈaratʃlɔskɛtə ɪst kˈapʊt."},  // compound split
        {"Kurz vor acht Uhr.", "kˈʊɐʦ foːɾ ˈaxt ˈuːɾ."},
        {"Wie geht's?", "viː ɡˈeːts?"},
        {"Phonemizers", "fˈɔnɛmiːɾs"},  // not in the dictionary: g2p_de GRU
    };
    for (const Case& c : cases) {
        const std::string input = c.input;
        CAPTURE(input);
        CHECK(german_to_phonemes(input, NumberLanguage::Auto, *g_de) == std::string(c.expected));
    }
}

TEST_CASE("german_number_format") {
    const auto ph = [](const char* text) { return german_to_phonemes(text, NumberLanguage::Auto, *g_de); };
    CHECK(ph("1.000 und 3,5") == ph("eintausend und drei komma fünf"));
    CHECK(ph("z. B.") == ph("zum Beispiel"));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: test_g2p <dict_dir> [doctest options]\n");
        return 2;
    }

    const std::string dir = std::string(argv[1]) + "/";
    const PhonemizerConfig cfg;
    try {
        g_proc = std::make_shared<JiebaProcessor>(dir + cfg.jieba_dict, dir + cfg.hmm_model, dir + cfg.user_dict,
                                                  dir + cfg.idf_path, dir + cfg.stop_word_path,
                                                  dir + cfg.pinyin_char, dir + cfg.pinyin_phrase);
        const auto eng = std::make_shared<const EnG2P>(dir + cfg.cmu_dict, dir + cfg.user_en_dict,
                                                       dir + cfg.g2p_en_model);
        g_g2p = std::make_unique<ZHG2P>(g_proc, eng);
        g_en = std::make_unique<EnglishPhonemizer>(eng);
        g_de = std::make_unique<GermanLexicon>(dir + cfg.de_dict, dir + cfg.g2p_de_model);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "failed to load G2P: %s\n", e.what());
        return 1;
    }

    std::vector<char*> dt_args{argv[0]};
    dt_args.insert(dt_args.end(), argv + 2, argv + argc);
    doctest::Context context;
    context.applyCommandLine(static_cast<int>(dt_args.size()), dt_args.data());
    return context.run();
}
