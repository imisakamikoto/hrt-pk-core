// C# binding for hrt_core — .NET 6+ / .NET Framework 4.7.2+ (x64).
//
// Build the native library first (`cargo build --release`) and put
// hrt_core.dll next to your executable, or add its folder to PATH.
//
//     var core = new HrtCore();
//     var curve = core.Simulate(new {
//         events = new[] {
//             new { id = "a", route = "injection", timeH = 0.0, doseMG = 4.0, ester = "EV" }
//         },
//         bodyWeightKg = 70.0,
//         nowH = 72.0,
//     });
//     Console.WriteLine(curve?["result"]?["e2PgMl"]?.AsArray().Max(n => n!.GetValue<double>()));
//
// Requests are serialised with System.Text.Json using camelCase, which is the
// spelling the library expects.

using System;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace HrtCore;

public sealed class HrtCoreException : Exception
{
    public HrtCoreException(string code, string message) : base($"{code}: {message}")
    {
        Code = code;
    }

    /// <summary>Machine-readable reason, e.g. <c>badRequest</c>.</summary>
    public string Code { get; }
}

public sealed class HrtCore : IDisposable
{
    private const string Lib = "hrt_core";

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_simulate(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_stats(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_calibrate(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_steady_state(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_plan_dose(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_reminders(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_adherence(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_supply(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_convert(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_catalog(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_sublingual(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_patches(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_safety(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_sensitivity(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_batch(IntPtr json);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr hrt_capabilities();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern int hrt_abi_version();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    private static extern void hrt_free(IntPtr pointer);

    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull,
    };

    public int AbiVersion => hrt_abi_version();

    public string Version
    {
        get
        {
            var ptr = hrt_version();
            try
            {
                var envelope = Parse(ptr) as JsonObject;
                return envelope?["result"]?.GetValue<string>() ?? "unknown";
            }
            finally
            {
                hrt_free(ptr);
            }
        }
    }

    public JsonObject Capabilities()
    {
        var ptr = hrt_capabilities();
        try
        {
            return Unwrap(ptr) as JsonObject ?? new JsonObject();
        }
        finally
        {
            hrt_free(ptr);
        }
    }

    // --- calls ------------------------------------------------------------

    public JsonNode? Simulate(object request) => Call(hrt_simulate, request);

    public JsonNode? Stats(object request) => Call(hrt_stats, request);

    public JsonNode? Calibrate(object request) => Call(hrt_calibrate, request);

    public JsonNode? SteadyState(object request) => Call(hrt_steady_state, request);

    public JsonNode? PlanDose(object request) => Call(hrt_plan_dose, request);

    public JsonNode? Reminders(object request) => Call(hrt_reminders, request);

    public JsonNode? Adherence(object request) => Call(hrt_adherence, request);

    public JsonNode? Supply(object request) => Call(hrt_supply, request);

    public JsonNode? Convert(object request) => Call(hrt_convert, request);

    public JsonNode? Catalog() => Call(hrt_catalog, new { });

    public JsonNode? Sublingual(object request) => Call(hrt_sublingual, request);

    public JsonNode? Patches(object request) => Call(hrt_patches, request);

    public JsonNode? Safety(object request) => Call(hrt_safety, request);

    public JsonNode? Sensitivity(object request) => Call(hrt_sensitivity, request);

    /// <summary>Several calls at once; individual failures stay inline.</summary>
    public JsonNode? Batch(object request) => Call(hrt_batch, request);

    // --- plumbing ---------------------------------------------------------

    private static JsonNode? Call(Func<IntPtr, IntPtr> native, object request)
    {
        var json = JsonSerializer.Serialize(request, Options);
        var payload = Marshal.StringToCoTaskMemUTF8(json);
        IntPtr result;
        try
        {
            result = native(payload);
        }
        finally
        {
            Marshal.FreeCoTaskMem(payload);
        }

        if (result == IntPtr.Zero)
        {
            throw new HrtCoreException("null", "the library returned a null pointer");
        }
        try
        {
            return Unwrap(result);
        }
        finally
        {
            hrt_free(result);
        }
    }

    private static JsonNode? Unwrap(IntPtr pointer)
    {
        var envelope = Parse(pointer) as JsonObject
                       ?? throw new HrtCoreException("badEnvelope", "response was not an object");
        if (envelope["ok"]?.GetValue<bool>() != true)
        {
            throw new HrtCoreException(
                envelope["code"]?.GetValue<string>() ?? "unknown",
                envelope["error"]?.GetValue<string>() ?? "unknown error");
        }
        return envelope["result"];
    }

    private static JsonNode? Parse(IntPtr pointer)
    {
        var text = Marshal.PtrToStringUTF8(pointer);
        return text is null ? null : JsonNode.Parse(text);
    }

    /// <summary>The native library stays loaded for the process lifetime.</summary>
    public void Dispose()
    {
        // Nothing to release: the loader owns the module. Present so the type
        // can be used in a `using` declaration without a special case.
    }
}
