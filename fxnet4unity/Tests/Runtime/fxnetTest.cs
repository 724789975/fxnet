using fxnetlib.dllimport;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
public class NewBehaviourScript : MonoBehaviour
{
	// Start is called before the first frame update
	void Start()
	{
#if !UNITY_EDITOR
		DLLImport.StartIOModule();
#endif
		DLLImport.SetLogCallback(delegate (byte[] pData, int dwLen)
		{
			Debug.Log(pData);
		}
		);

		DLLImport.CreateSessionMake(delegate (IntPtr pConnector, byte[] pData, uint nLen)
		{
			Debug.LogFormat("{0} recv: {1}", pConnector, pData);
			DLLImport.Send(pConnector, pData, nLen);
		}, delegate (IntPtr pConnector)
		{
			Debug.LogFormat("{0} connected", pConnector);
		}, delegate (IntPtr pConnector, IntPtr nLen) { }, delegate (IntPtr pConnector)
		{
			Debug.Log("connector closed");
			DLLImport.DestroyConnector(pConnector);
		});
	}

	// Update is called once per frame
	void Update()
	{
		DLLImport.ProcessIOModule();
	}

	public void L()
	{
		DLLImport.TcpListen("0.0.0.0", 10085);
	}

	public void OnClick()
	{
		connector = DLLImport.CreateConnector(delegate (IntPtr pConnector, byte[] pData, uint nLen)
		{
			Debug.Log(pData);
		}, delegate (IntPtr pConnector)
		{
			Debug.LogFormat("{0} connected2222", pConnector);
		}, delegate (IntPtr pConnector, IntPtr nLen) { }, delegate (IntPtr pConnector)
		{
			Debug.Log("connector destroy");
			DLLImport.DestroyConnector(pConnector);
		});

		DLLImport.TcpConnect(connector, "127.0.0.1", 10085);
	}

	public void SendM()
	{
		string m = "1234";
		byte[]  str = Encoding.UTF8.GetBytes(m);
		DLLImport.Send(connector, str, (uint)str.Length);
	}

	IntPtr connector;
}
