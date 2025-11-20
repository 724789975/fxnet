using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using fxnetlib.dllimport;

using UnityEditor;
public class FxnetInit
{
	[InitializeOnLoadMethod]
	static void InitializeOnLoadMethod()
	{
		DLLImport.StartIOModule();
		UnityEngine.Debug.Log("FxnetInit.InitializeOnLoadMethod");
	}
}
