using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

namespace KspMission.RuntimeExporter
{
    [KSPAddon(KSPAddon.Startup.Flight, false)]
    public sealed class Exporter : MonoBehaviour
    {
        private sealed class BodyState
        {
            internal CelestialBody Body;
            internal BodyState Parent;
            internal double[] RelativePosition, RelativeVelocity, Position, Velocity;
        }

        private void Update()
        {
            if (!Input.GetKeyDown(KeyCode.F8) ||
                !(Input.GetKey(KeyCode.LeftControl) || Input.GetKey(KeyCode.RightControl)) ||
                !(Input.GetKey(KeyCode.LeftAlt) || Input.GetKey(KeyCode.RightAlt))) return;
            try
            {
                var path = Capture();
                ScreenMessages.PostScreenMessage("KSP Mission snapshot: " + path, 8f, ScreenMessageStyle.UPPER_CENTER);
            }
            catch (Exception error)
            {
                Debug.LogError("[KspMission.RuntimeExporter] " + error);
                ScreenMessages.PostScreenMessage("KSP Mission snapshot failed: " + error.Message, 10f,
                    ScreenMessageStyle.UPPER_CENTER);
            }
        }

        private static string Capture()
        {
            var gameUt = Planetarium.GetUniversalTime();
            if (double.IsNaN(gameUt) || double.IsInfinity(gameUt)) throw new InvalidOperationException("Game UT is not finite.");
            var saveId = HighLogic.SaveFolder;
            if (string.IsNullOrWhiteSpace(saveId)) throw new InvalidOperationException("A loaded save is required.");
            var adapterType = AppDomain.CurrentDomain.GetAssemblies()
                .Select(a => a.GetType("principia.ksp_plugin_adapter.PrincipiaPluginAdapter", false))
                .FirstOrDefault(t => t != null);
            if (adapterType == null) throw new InvalidOperationException("Principia adapter is not loaded.");
            var adapter = UnityEngine.Object.FindObjectOfType(adapterType);
            if (adapter == null) throw new InvalidOperationException("Principia plugin instance is absent.");
            var flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
            var running = adapterType.GetMethod("PluginRunning", flags);
            var pluginMethod = adapterType.GetMethod("Plugin", flags);
            if (running == null || pluginMethod == null || !(bool)running.Invoke(adapter, null))
                throw new InvalidOperationException("Principia plugin is not running.");
            var plugin = (IntPtr)pluginMethod.Invoke(adapter, null);
            if (plugin == IntPtr.Zero) throw new InvalidOperationException("Principia plugin pointer is empty.");
            var interfaceType = adapterType.Assembly.GetType("principia.ksp_plugin_adapter.Interface", true);
            var currentTime = FindMethod(interfaceType, "CurrentTime", 1);
            var fromParent = FindMethod(interfaceType, "CelestialFromParent", 2);
            var pluginUt = Convert.ToDouble(currentTime.Invoke(null, new object[] { plugin }), CultureInfo.InvariantCulture);
            if (!Finite(pluginUt) || Math.Abs(pluginUt - gameUt) > 0.001)
                throw new InvalidOperationException("Principia and game UT differ; wait for synchronization.");

            var bodies = FlightGlobals.Bodies;
            if (bodies == null || bodies.Count < 2) throw new InvalidOperationException("Loaded body list is incomplete.");
            var states = new Dictionary<CelestialBody, BodyState>();
            foreach (var body in bodies)
            {
                if (body == null || states.ContainsKey(body)) throw new InvalidOperationException("Null or duplicate body.");
                states.Add(body, new BodyState { Body = body });
            }
            foreach (var state in states.Values)
            {
                var parent = state.Body.referenceBody;
                if (parent != null && parent != state.Body)
                {
                    if (!states.TryGetValue(parent, out state.Parent))
                        throw new InvalidOperationException("Missing parent for " + state.Body.bodyName);
                    var qp = fromParent.Invoke(null, new object[] { plugin, state.Body.flightGlobalsIndex });
                    state.RelativePosition = Vector(Member(qp, "q"));
                    state.RelativeVelocity = Vector(Member(qp, "p"));
                }
                else
                {
                    state.RelativePosition = new double[3];
                    state.RelativeVelocity = new double[3];
                }
            }
            if (states.Values.Count(s => s.Parent == null) != 1)
                throw new InvalidOperationException("Loaded body hierarchy needs one root.");
            var active = new HashSet<BodyState>();
            foreach (var state in states.Values) Resolve(state, active);
            var totalMu = states.Values.Sum(s => s.Body.gravParameter);
            if (!Finite(totalMu) || totalMu <= 0) throw new InvalidOperationException("Gravitational parameters are invalid.");
            var comPosition = new double[3];
            var comVelocity = new double[3];
            for (var axis = 0; axis < 3; axis++)
            foreach (var state in states.Values)
            {
                var weight = state.Body.gravParameter / totalMu;
                comPosition[axis] += weight * state.Position[axis];
                comVelocity[axis] += weight * state.Velocity[axis];
            }
            foreach (var state in states.Values)
            for (var axis = 0; axis < 3; axis++)
            {
                state.Position[axis] -= comPosition[axis];
                state.Velocity[axis] -= comVelocity[axis];
            }

            var gameData = Path.Combine(KSPUtil.ApplicationRootPath, "GameData");
            var principiaPath = adapterType.Assembly.Location;
            var jnsqPath = Path.Combine(gameData, "JNSQ", "Version", "JNSQ.version");
            var rebornPath = Path.Combine(gameData, "JNSQ-Reborn", "JNSQReborn-Configuration.cfg");
            foreach (var path in new[] { principiaPath, jnsqPath, rebornPath })
                if (!File.Exists(path)) throw new InvalidOperationException("Required loaded-mod source file is missing: " + path);
            var gameVersion = Versioning.GetVersionStringFull();
            var json = Serialize(states.Values.OrderBy(s => s.Body.flightGlobalsIndex).ToArray(),
                pluginUt, saveId, gameVersion, Sha256(principiaPath), Sha256(jnsqPath), Sha256(rebornPath));
            var directory = Path.Combine(KSPUtil.ApplicationRootPath, "PluginData", "KspMission");
            Directory.CreateDirectory(directory);
            var fileName = "snapshot-" + pluginUt.ToString("F3", CultureInfo.InvariantCulture).Replace('.', '-') +
                "-" + DateTime.UtcNow.ToString("yyyyMMddTHHmmssfff", CultureInfo.InvariantCulture) + ".json";
            var target = Path.Combine(directory, fileName);
            var temporary = target + ".tmp";
            File.WriteAllText(temporary, json, new UTF8Encoding(false));
            File.Move(temporary, target);
            return target;
        }

        private static void Resolve(BodyState state, HashSet<BodyState> active)
        {
            if (state.Position != null) return;
            if (!active.Add(state)) throw new InvalidOperationException("Loaded body parent cycle.");
            if (state.Parent != null) Resolve(state.Parent, active);
            state.Position = Add(state.Parent == null ? new double[3] : state.Parent.Position, state.RelativePosition);
            state.Velocity = Add(state.Parent == null ? new double[3] : state.Parent.Velocity, state.RelativeVelocity);
            active.Remove(state);
        }
        private static double[] Add(double[] a, double[] b) => new[] { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
        private static MethodInfo FindMethod(Type type, string name, int parameterCount)
        {
            var method = type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static)
                .SingleOrDefault(m => m.Name == name && m.GetParameters().Length == parameterCount);
            if (method == null) throw new InvalidOperationException("Principia API missing " + name + ".");
            return method;
        }
        private static object Member(object value, string name)
        {
            if (value == null) throw new InvalidOperationException("Principia returned a null value.");
            var flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;
            var type = value.GetType();
            var field = type.GetField(name, flags);
            if (field != null) return field.GetValue(value);
            var property = type.GetProperty(name, flags);
            if (property != null) return property.GetValue(value, null);
            throw new InvalidOperationException("Principia interchange member missing: " + name);
        }
        private static double[] Vector(object value)
        {
            var xyz = new[] { "x", "y", "z" }.Select(n => Convert.ToDouble(Member(value, n), CultureInfo.InvariantCulture)).ToArray();
            if (xyz.Any(x => !Finite(x))) throw new InvalidOperationException("Principia state is nonfinite.");
            return xyz;
        }
        private static bool Finite(double value) => !double.IsNaN(value) && !double.IsInfinity(value);
        private static string Sha256(string path)
        {
            using (var stream = File.OpenRead(path))
            using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        private static string Number(double value)
        {
            if (!Finite(value)) throw new InvalidOperationException("Nonfinite export field.");
            return value.ToString("R", CultureInfo.InvariantCulture);
        }
        private static string Quote(string value)
        {
            var result = new StringBuilder("\"");
            foreach (var c in value ?? "")
            {
                if (c == '\\' || c == '"') result.Append('\\').Append(c);
                else if (c < 32) result.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                else result.Append(c);
            }
            return result.Append('"').ToString();
        }
        private static string VectorJson(double[] value) => "[" + string.Join(",", value.Select(Number).ToArray()) + "]";
        private static string Serialize(BodyState[] states, double ut, string saveId, string gameVersion,
            string principiaHash, string jnsqHash, string rebornHash)
        {
            var b = new StringBuilder();
            b.Append("{\"schema_version\":1,\"confidence\":\"runtime_observed_uncompared\",\"capture\":{");
            b.Append("\"exporter_id\":\"KspMission.RuntimeExporter\",\"exporter_version\":\"1\",");
            b.Append("\"game_version\":").Append(Quote(gameVersion)).Append(",\"save_id\":").Append(Quote(saveId));
            b.Append(",\"mods\":[{\"id\":\"Principia\",\"version\":").Append(Quote("sha256:" + principiaHash));
            b.Append("},{\"id\":\"JNSQ\",\"version\":").Append(Quote("sha256:" + jnsqHash));
            b.Append("},{\"id\":\"JNSQ-Reborn\",\"version\":").Append(Quote("sha256:" + rebornHash));
            b.Append("}],\"capture_ut_s\":").Append(Number(ut));
            b.Append(",\"principia_loaded\":true,\"state_source\":\"principia_celestial_from_parent\"},");
            b.Append("\"frame\":{\"origin\":\"system_barycenter\",\"axes\":\"principia_alicesun_frozen_at_capture\",");
            b.Append("\"handedness\":\"right\",\"inertial\":true,\"source_frame\":\"Principia/AliceSun\",");
            b.Append("\"transform_method\":\"parent_relative_sum_then_com_translation\",\"transform_version\":\"1\"},\"bodies\":[");
            for (var i = 0; i < states.Length; i++)
            {
                var state = states[i];var body = state.Body;
                if (i != 0) b.Append(',');
                b.Append("{\"id\":").Append(Quote(body.bodyName)).Append(",\"parent_id\":")
                    .Append(state.Parent == null ? "null" : Quote(state.Parent.Body.bodyName));
                b.Append(",\"mu_m3_s2\":").Append(Number(body.gravParameter));
                b.Append(",\"radius_m\":").Append(Number(body.Radius));
                b.Append(",\"atmosphere_boundary_m\":").Append(body.atmosphere ? Number(body.atmosphereDepth) : "null");
                b.Append(",\"state_epoch_ut_s\":").Append(Number(ut));
                b.Append(",\"position_m\":").Append(VectorJson(state.Position));
                b.Append(",\"velocity_mps\":").Append(VectorJson(state.Velocity)).Append('}');
            }
            b.Append("],\"calendar\":{\"day_duration_s\":86400,\"display_origin_ut_s\":0,\"use_leap_years\":false,");
            b.Append("\"month_lengths\":[31,28,31,30,31,30,31,31,30,31,30,31]}}");
            return b.ToString();
        }
    }
}
