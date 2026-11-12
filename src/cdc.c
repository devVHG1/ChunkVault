#include "cdc.h"

#include <string.h>

/* SplitMix64 output sequence initialized with state 0x43564d414e494645. */
static const uint64_t gear[256] = {
    UINT64_C(0x83d82149c8f564e7), UINT64_C(0xd63e47e7687b5e92), UINT64_C(0xb14f8f3ecb1caddb), UINT64_C(0x7eff0aa49018f494),
    UINT64_C(0x561f66ebf8bf0ef5), UINT64_C(0x21a4163901d8bd8f), UINT64_C(0x534448b4a7aff2ff), UINT64_C(0xdaeeeaf317a4bd02),
    UINT64_C(0x7b85128d4b95c692), UINT64_C(0x03453efdaac6dfd0), UINT64_C(0x4344f0fea2a2570c), UINT64_C(0x8f66cc16ac083d63),
    UINT64_C(0xd4fba4c65bb056d3), UINT64_C(0xf0040e296bbcc577), UINT64_C(0x5071601515e81332), UINT64_C(0x5201a41b8aaf5ea4),
    UINT64_C(0x1874b98e95f043fb), UINT64_C(0x1787269750194c44), UINT64_C(0xd19331c7ca179cbf), UINT64_C(0x7d93f910bd87ab06),
    UINT64_C(0x5a3a6d7fa2744642), UINT64_C(0x9da5765f253cde52), UINT64_C(0x701c3e99047f4db8), UINT64_C(0x98f8a1e9dd3ec0ff),
    UINT64_C(0xc932e821bb3f067a), UINT64_C(0xe635523de57c86b5), UINT64_C(0xec65bea39a517755), UINT64_C(0x391fca887b594b2c),
    UINT64_C(0x1c2cecf71dbb7688), UINT64_C(0x047185669beda6f1), UINT64_C(0x148964e869f5a760), UINT64_C(0x067d81c9d15770c5),
    UINT64_C(0xc975619b5e7a4464), UINT64_C(0x751e0b010176fc78), UINT64_C(0x4a600f5d14a3c5fb), UINT64_C(0xe7ecac60a6047861),
    UINT64_C(0x2f8d0d8859e8ff09), UINT64_C(0x5d7a07466194cb70), UINT64_C(0x538ec596c798b45d), UINT64_C(0xe0aa2382e9d93499),
    UINT64_C(0x9890316d2d70883b), UINT64_C(0x9b0bd1fb391bd571), UINT64_C(0xc887c37555660acb), UINT64_C(0x71eb1db0516c53a5),
    UINT64_C(0x5bd0d8e7fa503f55), UINT64_C(0xe7b91cf65ce94121), UINT64_C(0x63b0c88dec21f28b), UINT64_C(0xa971099ea651bacb),
    UINT64_C(0x20e9f68b64ab9db4), UINT64_C(0x7f048bd372d22319), UINT64_C(0xd8e08f6bb3c2ce60), UINT64_C(0x1864e1a90adce833),
    UINT64_C(0xa2d281dea4be292b), UINT64_C(0xdb364c5152b1c35c), UINT64_C(0x54918570cfd91a28), UINT64_C(0x04c8f80126da56a4),
    UINT64_C(0xf735a69e019ffe00), UINT64_C(0xd10313de17344b23), UINT64_C(0x478086962cfd25a1), UINT64_C(0xdcb406075523c8fd),
    UINT64_C(0x16fa01167a06b94f), UINT64_C(0x730366d5d9d85a4f), UINT64_C(0xdbc2980fb8a73e53), UINT64_C(0x4b93e10e11fc7390),
    UINT64_C(0x13bd2f71cfe2eb80), UINT64_C(0x037e3ef8d0e06b69), UINT64_C(0xb0c720476dc0f1a0), UINT64_C(0xf011dead8dde93be),
    UINT64_C(0xad3404f606b20376), UINT64_C(0x83f54d994e2a5c28), UINT64_C(0x365a32ad6df9f1a2), UINT64_C(0xaa0221a0a7acc93b),
    UINT64_C(0xdd8fb2016a8c878a), UINT64_C(0x9055315b5688f310), UINT64_C(0x04856aea23940aaf), UINT64_C(0x4bc61337227ff981),
    UINT64_C(0x48b93edb59b9a525), UINT64_C(0x3131a500ccfe8b14), UINT64_C(0x382787e92b5d6842), UINT64_C(0x048313fce5999dbb),
    UINT64_C(0x61db88a737c50834), UINT64_C(0xfcfbe3e85907f8d0), UINT64_C(0x707f02b2c196d8db), UINT64_C(0xeade234346ff64ac),
    UINT64_C(0xd0b19b761de6415c), UINT64_C(0xd6f6cfe7d3a67f10), UINT64_C(0x44bba83f5655f137), UINT64_C(0x8f8b9dc7379a381c),
    UINT64_C(0xc9672574f7fe47f7), UINT64_C(0x37b8c710c7ca79aa), UINT64_C(0x9a8c3112ff62be1c), UINT64_C(0xdccd606ca6b99045),
    UINT64_C(0xc21fa7b469cc0774), UINT64_C(0x718130b93a1509f4), UINT64_C(0x0093fe795268a785), UINT64_C(0x5db9d2756ef069d4),
    UINT64_C(0x2d4042527147a52a), UINT64_C(0xe220ddaf388533e9), UINT64_C(0xd2462bb54f1537e9), UINT64_C(0x50b8c60fa0707ea7),
    UINT64_C(0x0b8c4a774dfe3c8e), UINT64_C(0x08d20d43f271bceb), UINT64_C(0x3e79a30801b22dc4), UINT64_C(0xbcb35dee2d5131da),
    UINT64_C(0x072f6d05736b7dfa), UINT64_C(0x4ca805db16edc125), UINT64_C(0xf3f9f522d34b3866), UINT64_C(0x434bbf56d95b2aa0),
    UINT64_C(0xeeec61ecd74cabd3), UINT64_C(0x020626d111ee3a21), UINT64_C(0x71c632bfb97b5691), UINT64_C(0x496e94a961dae6e9),
    UINT64_C(0x9e186bd3f05752a8), UINT64_C(0xe0d649c5b710f3b1), UINT64_C(0x9187f6dde5603a86), UINT64_C(0xdd1f6f9b4d46ccae),
    UINT64_C(0xc464400fb0be8b55), UINT64_C(0xd8bc5075ae483172), UINT64_C(0xdf260ad780f36cda), UINT64_C(0xa2d7b85cb0b76b8d),
    UINT64_C(0x82b819b04ddbf8da), UINT64_C(0x71a6060a0b4c9831), UINT64_C(0xa7df57b486478686), UINT64_C(0xeb510f2007f2bf5a),
    UINT64_C(0xc8f3f50206455c76), UINT64_C(0xfb18b94ec537bcb6), UINT64_C(0x1b32efedd9f3e288), UINT64_C(0xbb9e890e41bb1d29),
    UINT64_C(0x226b94cb20017655), UINT64_C(0x8dedf20768e1f252), UINT64_C(0x0ba3247958b763a9), UINT64_C(0x84a36d1ba5ae48b0),
    UINT64_C(0xe23aa816675bd576), UINT64_C(0x71236ee487d6c7b7), UINT64_C(0x1dd978bc8a4708f0), UINT64_C(0xd488be82b223501f),
    UINT64_C(0x2fc3957b7e7f6d00), UINT64_C(0xc8dd8d0301c8c939), UINT64_C(0xad43d8a3bb5a7608), UINT64_C(0x1a274e8d384ed2b6),
    UINT64_C(0x1d97dfffc5415edc), UINT64_C(0xcdc324aa91f1f90d), UINT64_C(0x945fc622dd2a6975), UINT64_C(0xdd27adcf85f5faa9),
    UINT64_C(0x9efa34cd60d0a7c2), UINT64_C(0xa3ccdbbdb41ec79f), UINT64_C(0xa90f620571dbd575), UINT64_C(0x9133f2355d8a1bb9),
    UINT64_C(0x036471c46d1916ab), UINT64_C(0x91c6c20c006c6f32), UINT64_C(0x929fcf963f75a164), UINT64_C(0xc3cb06fc0edb4ae8),
    UINT64_C(0xbc819985f4c971d5), UINT64_C(0x2d7f168045f92fc5), UINT64_C(0x72fac9721de26f94), UINT64_C(0x35812dccf27f71f0),
    UINT64_C(0x422da2da6274abfa), UINT64_C(0x08aae6f98ddb3a02), UINT64_C(0x537c1b313da899ae), UINT64_C(0x4dc6542429dfd313),
    UINT64_C(0x065007751fa7fa12), UINT64_C(0x3c0bab8b8fda4195), UINT64_C(0xf0e19c8b06cd8860), UINT64_C(0x45184deca9494738),
    UINT64_C(0xcca946904293e905), UINT64_C(0xddf48c3374ba0817), UINT64_C(0x4854190039911467), UINT64_C(0x24d5646c4dfa2552),
    UINT64_C(0xf8278b25e07f77f0), UINT64_C(0xd0776086aca3d1b9), UINT64_C(0x2e0c52103fda6e36), UINT64_C(0x12c56587d489a8bd),
    UINT64_C(0xde90a6d1e1308b07), UINT64_C(0xad226d95263ac2a1), UINT64_C(0xd43aed606456cc14), UINT64_C(0x4d409a7121083445),
    UINT64_C(0x5f4d2f72acb29d25), UINT64_C(0xfd584811d24d97f1), UINT64_C(0xd7574dd111b16fdc), UINT64_C(0xfeb63215696ad113),
    UINT64_C(0x97168cc313ed9394), UINT64_C(0xd0ea7eb5c7a45426), UINT64_C(0x9a59e0b344b62d51), UINT64_C(0x75c56b791c57c65b),
    UINT64_C(0x67b2b0578965bfa3), UINT64_C(0x1e53d0a4fbdb4986), UINT64_C(0x7174ffe32269b2d3), UINT64_C(0xedc2f7408d6379e3),
    UINT64_C(0x33b92c461e1eafde), UINT64_C(0x46d430e7086c0135), UINT64_C(0x10cb9713b9370dbf), UINT64_C(0xc173d2a946440b9e),
    UINT64_C(0x83ec81b50cd4be3e), UINT64_C(0x8db3c086851c5731), UINT64_C(0xdcaeb30be9f6381b), UINT64_C(0x8c7d13bac4031eca),
    UINT64_C(0xc61ebe016b00ea0c), UINT64_C(0xf465805496bbdb10), UINT64_C(0x582ba10539c199a5), UINT64_C(0x9878b83a3302c7b8),
    UINT64_C(0x48b62a979b3626d2), UINT64_C(0x2c0cffd89258bc7b), UINT64_C(0xa750e9d20edf265a), UINT64_C(0xd0c5f98d6b8f6f93),
    UINT64_C(0xf9a7bebb160d8ad3), UINT64_C(0xa158a1a0a6d60c1e), UINT64_C(0x8c563d352ac7f9d2), UINT64_C(0x9830065aaa001bc5),
    UINT64_C(0x49b8beae8997a9cd), UINT64_C(0xe29059b81fd5473b), UINT64_C(0xe9f0a793582277dd), UINT64_C(0x936a6e0c23467a26),
    UINT64_C(0x42b3a3c13d2f7665), UINT64_C(0xa44c94a9d16d0cc3), UINT64_C(0xfa0c051906e7a3ef), UINT64_C(0xd1220376a776f239),
    UINT64_C(0xbfa36842c051a60e), UINT64_C(0xc0051ac01a663ae9), UINT64_C(0x2d1e35378d27198d), UINT64_C(0x22b2137f78ff0396),
    UINT64_C(0xa7b45f25e5fb2a85), UINT64_C(0xa5f603e32aa4c1da), UINT64_C(0x96e7bf05dcf7d658), UINT64_C(0x6d5016d067fc151d),
    UINT64_C(0x65eff0983fece8ad), UINT64_C(0x52527e4b8ed2e1ed), UINT64_C(0xbc37a9650839f706), UINT64_C(0x135e7f05663de61d),
    UINT64_C(0xa5a784ae6c194deb), UINT64_C(0x9f5a130de02287e0), UINT64_C(0x169c784932b5c28b), UINT64_C(0xdbe3c6a9c56e31a4),
    UINT64_C(0x8664c1d4115d35b7), UINT64_C(0x828aed4c810eb039), UINT64_C(0x0c879bf7d587acda), UINT64_C(0x21313040b59f422b),
    UINT64_C(0xa6235df085a6b416), UINT64_C(0xda5e46cce3fd3e47), UINT64_C(0xf46fdabe5cd68700), UINT64_C(0x969fc7d149ebf4a6),
    UINT64_C(0xec1b71684352ad3f), UINT64_C(0x9ceda8a397425ee7), UINT64_C(0x01314f8afeea32d2), UINT64_C(0xa65e5c56e6d565c4),
    UINT64_C(0xf9060e9179e0915f), UINT64_C(0x19bf11b76b996beb), UINT64_C(0x9638a02c01c2b606), UINT64_C(0x7936c3a142a5dc43),
    UINT64_C(0x4076c108ae8e6467), UINT64_C(0xec6d7a4b7cf84dac), UINT64_C(0x66b67e1dc3d0061b), UINT64_C(0xc8d5b3a6787670cb),
    UINT64_C(0xe67ae1435259ef7d), UINT64_C(0x4ba3c0d889ef2b5f), UINT64_C(0x565927f2d65874a2), UINT64_C(0xe99e4f8ca06204e9)
};

static size_t boundary(const unsigned char *bytes, size_t size)
{
    if (size <= CV_CDC_MIN) {
        return size;
    }
    uint64_t hash = 0;
    for (size_t i = CV_CDC_MIN; i < size; ++i) {
        hash = (hash << 1) + gear[bytes[i]];
        uint64_t mask = i < CV_CDC_TARGET ? UINT64_C(0x1ffff) : UINT64_C(0x7fff);
        if ((hash & mask) == 0) {
            return i + 1;
        }
    }
    return size;
}

enum cv_chunk_result cv_cdc_read(struct cv_cdc *reader, FILE *input,
                                 const unsigned char **bytes, size_t *size,
                                 unsigned char digest[CV_SHA256_SIZE])
{
    *size = 0;
    *bytes = NULL;
    if (ferror(input)) {
        return CV_CHUNK_ERROR;
    }
    if (reader->consumed) {
        reader->used -= reader->consumed;
        if (reader->used) {
            memmove(reader->bytes, reader->bytes + reader->consumed, reader->used);
        }
    }
    reader->consumed = 0;
    reader->used += fread(reader->bytes + reader->used, 1,
                          sizeof reader->bytes - reader->used, input);
    if (ferror(input)) {
        return CV_CHUNK_ERROR;
    }
    if (reader->used == 0) {
        return CV_CHUNK_END;
    }

    struct cv_sha256 hash;
    reader->consumed = boundary(reader->bytes, reader->used);
    cv_sha256_init(&hash);
    cv_sha256_update(&hash, reader->bytes, reader->consumed);
    cv_sha256_finish(&hash, digest);
    *bytes = reader->bytes;
    *size = reader->consumed;
    return CV_CHUNK_OK;
}
