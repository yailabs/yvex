/* Own the exact DeepSeek physical representations admitted by its compiler.
 * Catalog selection is not byte verification; binding admission hashes the file.
 */
#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/core.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/gguf.h>
#include <string.h>

typedef struct {
    unsigned long long snapshot, mapping;
    const char *payload;
} deepseek_artifact_source;

/* A family catalog is not one checkpoint: every physical row explicitly
 * retains its source lineage, including historical representations. */
static const deepseek_artifact_source deepseek_dspark_source = {
    0x8d8da435dea23049ull, 0x779aa44d104fc718ull,
    "e05ddb86f9783bf665d05395636588f4e8dbd1ee6f1ba54be4140f84369ee939"};
static const deepseek_artifact_source deepseek_0731_source = {
    0x8d8da435dea23049ull, 0x779aa44d104fc718ull,
    "d46c2f3a4305155f357191e0bd804defe33764b92997c3de123b3b5cab41e5ce"};

typedef struct {
    const deepseek_artifact_source *source;
    unsigned long long payload_bytes, file_bytes;
    const char *transform, *profile, *name, *quant, *payload_plan;
    const char *payload_bytes_id, *writer, *artifact;
} deepseek_artifact_variant;

static const deepseek_artifact_variant deepseek_artifact_catalog[] = {
    /* Program P experiment: native emission/roundtrip and independent pinned
     * reader evidence authenticate this exact file. Not a quality or speed
     * recommendation, and not permission to admit arbitrary planner output. */
    {&deepseek_0731_source, 103717174264ull, 103728880544ull,
     YVEX_DEEPSEEK_0731_TRANSFORM_IDENTITY,
     "572c0689fdc28e093656ba6986f18d1410a85891c690c3acad98f0d9269c4b8a",
     "goal-v1-mxfp4-routed-q2_k",
     "e460eb3e3e30c712912dc5810bbd12f269f8cd44b599cba173a13872994a7710",
     "49e3ed3d77650ddea0f9d6ff25ff9c8610c15ff949a69de8f3edc3e12d7c45d0",
     "8637ec092f1ab95b4874e288b720730d099161384f79da52b67624fde6b95a5a",
     "20d50ed22b8cc286d62395e128b14531d361fca92adc7349a5dd3e774938b190",
     "25d29155aa86505ce0aa5a1c5535f9ed6e3b073d1803409a646b1f643423f041"},
    {&deepseek_0731_source, 102436666360ull, 102448372608ull,
     YVEX_DEEPSEEK_0731_TRANSFORM_IDENTITY,
     "bdb68a95537525560d5af218975cc6d560ad0c93013d970f91925c2d2629a028",
     "goal-v1-q2_k",
     "3258e5f54107ec7aa2cb98c7fa515d74bfa6c3d968ee8eeab0114072813d0279",
     "8b8e3e240ddb68ae3330455f8c23fa34a562914f36ce636aa8ec0c1e6fbc03ad",
     "2dae2252f9899af87a6a77118e4cafbd70ff38d876ce37d8b5e8c969d74e4558",
     "76ad99a2657f4f488ec0785d16feaebb99c3ff41a3b0439addcba81a00515c12",
     "7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f"},
    {&deepseek_0731_source, 107066194936ull, 107077901216ull,
     YVEX_DEEPSEEK_0731_TRANSFORM_IDENTITY,
     "09312d58916c4081ef9091e41bfd35caecc51ed698847a44c2f25740f3db1024",
     YVEX_DEEPSEEK_0731_QUANT_PRESET,
     "6ed4f6dd5656e009f4e53ad3731f99560551813de8e6c241e2c0bd8c1307ec88",
     "90e714a2b1f28adbfcdde935d1c56f1e5b7e75605a36b592b91130f740b8a775",
     "3881a6aea47fe1a96c8892148bcc3e592c99aaad25de6cff6413e950ce3baa28",
     "d7f99c3431600567251a1107f6b0e7bced7ea30c678db214a6d5dfa7118a7005",
     "4dc4265a92d77c874c82aa16c1358688b71bb7b10267cc80911c3ef42d2fa11a"},
    {&deepseek_dspark_source, 108274154488ull, YVEX_SELECTED_DEEPSEEK_FILE_BYTES,
     YVEX_DEEPSEEK_LEGACY_ARTIFACT_TRANSFORM_IDENTITY,
     "a48d43c8594999a1af3a5b1f572b34a5823042cb767832d558642bb804b036c5",
     "deepseek-v4-flash-dspark-bootstrap-q2-v1",
     "777559149e4e8421c34299da78f63f6b0d296a91005d7670196164c3c72b62af",
     "8d1a89e794363c0aaf1c721b07c0661ea03f9680691d0113543b2540297b69e7",
     "6dce1edb82810715687d40c6d62273e992cfe9e0aa610cb9598447e06fb7099f",
     "1ba1ceaa709862145b1a145e938cf03327cd58da27bca42ade2f884e2b2fc635",
     "bf80bd7372e9ff754cd61d8f6e849ca8eff2177fad40840a2dad8e840b35690a"},
    {&deepseek_dspark_source, 98006498296ull, 98018204640ull,
     YVEX_DEEPSEEK_LEGACY_ARTIFACT_TRANSFORM_IDENTITY,
     "6a99e9f7c374e3f718cce705002bf2b799db9cc1b86f65091631857f52c1c587",
     "deepseek-v4-flash-dspark-native-drafter-candidate",
     "35002244d5854a2d51b877ea31614cd985c9795d11c7e0904ed3475fec7fcb77",
     "e83545c729b219d327d4a437d499b73407648c94748ba7fda13905baace15c3e",
     "c79712bb85e31ebdcbd71ef0256709a001ae4cc62c4150ba8726d5dc5722dcd0",
     "2d4694925c02c04811ea846f389a94dbf524d26809a292c93f2c46ca8f05a025",
     "59c4649b19bb9f3eb7c01559e12ae52c3d4fbd067957e35de0a1a851759c7cc1"},
    {&deepseek_dspark_source, 95038503928ull, 95050210304ull,
     YVEX_DEEPSEEK_CURRENT_LOGICAL_TRANSFORM_IDENTITY,
     "b9825a070028a66af28cdb25614f7a86c6ad1ec396eed6ae961039db1507ce0e",
     "deepseek-v4-flash-dspark-compact-selective-mxfp4-candidate",
     "ca591438ac7296fa9b3d1ad74415508d57d92835ab783d01b7da9bfec561e8d7",
     "2cf8c2c41cb13ad228a2193e55ae56da210a7b06d356ec74ad79155a3e1cd1e0",
     "85b52eae100a482f557611ac9b5e84e9bd133525d01a3a7a27fc7520aa819fd5",
     "c4484184f0d4b3aeba9ae306b3247f4e3134e734ecfa6cd0f5d79ac24c524bce",
     "d27b87a9e7c7959c442b0231621588d274f22a8aa916cb05750508cd39ff6f53"},
    {&deepseek_dspark_source, 98006498296ull, 98018204640ull,
     YVEX_DEEPSEEK_CURRENT_LOGICAL_TRANSFORM_IDENTITY,
     "4aac0961d3159f8a3d585cd4b08e2d15115c3577ca9df080875238bb79290b2c",
     "deepseek-v4-flash-mixed-iq2xxs-q2k-q8-v1",
     "e62cff309e28328527e3e8f3d59e495b1fc42d8c882c6768b0efaabf16d43db8",
     "e83545c729b219d327d4a437d499b73407648c94748ba7fda13905baace15c3e",
     "c79712bb85e31ebdcbd71ef0256709a001ae4cc62c4150ba8726d5dc5722dcd0",
     "5ee95f33d4ecff5ee7a1c0f6b4c670715191ae1536fcda54afa2a6c00faa663a",
     "51f459af06e60efd411b308aa4ece518f3730b1023a95f86e20e8c2a5422b706"},
    {&deepseek_dspark_source, 95038503928ull, 95050210272ull,
     YVEX_DEEPSEEK_CURRENT_LOGICAL_TRANSFORM_IDENTITY,
     "59dd7bdabf6b81989dfa14e0f70692805a8f02a473afcc040a3e55083f48dda0",
     "deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1",
     "4036b8ce06ae4eb47e825e18585155bf3ae94c3bef715d7b79b9603cd8a02fff",
     "2cf8c2c41cb13ad228a2193e55ae56da210a7b06d356ec74ad79155a3e1cd1e0",
     "85b52eae100a482f557611ac9b5e84e9bd133525d01a3a7a27fc7520aa819fd5",
     "905e6432ee804100367cf846cced3dd06d4757bb78bb8aa8c2d32cbe341e44b3",
     "b669d80726cf83331c0d8016debbde44cf965a1503c33f605e92ea4e550ee87f"},
};

static int deepseek_catalog_find(unsigned long long file_bytes, const char *profile,
                                 yvex_complete_artifact_admission *out)
{
    size_t index;

    for (index = 0u; index < sizeof(deepseek_artifact_catalog) /
                                      sizeof(deepseek_artifact_catalog[0]);
         ++index) {
        const deepseek_artifact_variant *row = &deepseek_artifact_catalog[index];

        if (row->file_bytes != file_bytes ||
            memcmp(row->profile, profile, YVEX_SHA256_HEX_CAP - 1u) != 0)
            continue;
        *out = (yvex_complete_artifact_admission){
            .artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX,
            .metadata_count = 76ull, .tensor_count = 1409ull,
            .payload_bytes = row->payload_bytes, .file_bytes = row->file_bytes,
            .source_snapshot_identity = row->source->snapshot,
            .mapping_identity = row->source->mapping,
            .tokenizer_complete = 1, .native_reader_accepted = 1,
            .official_reader_accepted = 1, .payload_integrity_accepted = 1,
            .materialization_input_ready = 1};
        yvex_core_text_copy(out->payload_identity, sizeof(out->payload_identity), row->source->payload);
        yvex_core_text_copy(out->transform_identity, sizeof(out->transform_identity), row->transform);
        yvex_core_text_copy(out->profile_identity, sizeof(out->profile_identity), row->profile);
        yvex_core_text_copy(out->profile_name, sizeof(out->profile_name), row->name);
        yvex_core_text_copy(out->quant_execution_identity,
                            sizeof(out->quant_execution_identity), row->quant);
        yvex_core_text_copy(out->payload_plan_identity,
                            sizeof(out->payload_plan_identity), row->payload_plan);
        yvex_core_text_copy(out->payload_byte_identity,
                            sizeof(out->payload_byte_identity), row->payload_bytes_id);
        yvex_core_text_copy(out->writer_plan_identity,
                            sizeof(out->writer_plan_identity), row->writer);
        yvex_core_text_copy(out->artifact_identity, sizeof(out->artifact_identity), row->artifact);
        yvex_core_text_copy(out->official_reader_revision,
                            sizeof(out->official_reader_revision),
                            YVEX_GGUF_OFFICIAL_READER_REVISION);
        return 1;
    }
    return 0;
}

int yvex_artifact_admit_deepseek(
    const yvex_artifact *artifact, yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure, yvex_error *err)
{
    yvex_complete_artifact_admission catalog;
    yvex_gguf *gguf = NULL;
    const yvex_gguf_value *value;
    const char *profile = NULL;
    unsigned long long length = 0ull;
    int found = 0;
    yvex_artifact_catalog_contract contract = {0};

    if (!artifact || !out)
        return yvex_artifact_admit_catalog(
            artifact, NULL, NULL, &contract, out, failure, err);
    if (yvex_gguf_open(&gguf, artifact, err) == YVEX_OK) {
        value = yvex_gguf_metadata_find(gguf, "yvex.quant.profile.identity");
        if (value && yvex_gguf_value_as_string(value, &profile, &length) == YVEX_OK &&
            length == YVEX_SHA256_HEX_CAP - 1u)
            found = deepseek_catalog_find(yvex_artifact_size(artifact), profile, &catalog);
    }
    yvex_gguf_close(gguf);
    if (!found) {
        memset(out, 0, sizeof(*out));
        if (failure) {
            memset(failure, 0, sizeof(*failure));
            failure->code = YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH;
            failure->actual = yvex_artifact_size(artifact);
            yvex_core_text_copy(failure->field, sizeof(failure->field), "profile-and-extent");
        }
        yvex_error_set(err, YVEX_ERR_FORMAT, "model.deepseek.artifact-catalog",
                       "artifact profile and extent are not in the admitted DeepSeek physical catalog");
        return YVEX_ERR_FORMAT;
    }
    contract.catalog = &catalog;
    return yvex_artifact_admit_catalog(
        artifact, NULL, NULL, &contract, out, failure, err);
}
