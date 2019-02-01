using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DrawBounds : MonoBehaviour {
    Rect[] recs;
    Color[] cs;
    string[] names;
   
    // Update is called once per frame
    void Update () {
        Application.CaptureScreenshot("unity-roi.png");
        GameObject[] items = GameObject.FindGameObjectsWithTag("item");
        GameObject player = GameObject.FindGameObjectWithTag("Player");
        recs = new Rect[items.Length+1];
        cs = new Color[items.Length + 1];
        names = new string[items.Length + 1];
        Rect boundPlayer = GUIRectWithObject(player);
        recs[0] = boundPlayer;
        cs[0] = Color.red;
        names[0] = "Player";        
        for (int i = 0; i < items.Length; i++) {
            Rect bounditem = GUIRectWithObject(items[i]);
            recs[i + 1] = bounditem;
            cs[i + 1] = Color.blue;
            names[i + 1] = "Item";            
        }
    }
    public static Rect GUIRectWithObject(GameObject go)
    {
        Vector3 cen = go.GetComponentInChildren<Collider>().bounds.center;
        Vector3 ext = go.GetComponentInChildren<Collider>().bounds.extents;
        Vector2[] extentPoints = new Vector2[8]
           {
               WorldToGUIPoint(new Vector3(cen.x-ext.x, cen.y-ext.y, cen.z-ext.z)),
               WorldToGUIPoint(new Vector3(cen.x+ext.x, cen.y-ext.y, cen.z-ext.z)),
               WorldToGUIPoint(new Vector3(cen.x-ext.x, cen.y-ext.y, cen.z+ext.z)),
               WorldToGUIPoint(new Vector3(cen.x+ext.x, cen.y-ext.y, cen.z+ext.z)),
               WorldToGUIPoint(new Vector3(cen.x-ext.x, cen.y+ext.y, cen.z-ext.z)),
               WorldToGUIPoint(new Vector3(cen.x+ext.x, cen.y+ext.y, cen.z-ext.z)),
               WorldToGUIPoint(new Vector3(cen.x-ext.x, cen.y+ext.y, cen.z+ext.z)),
               WorldToGUIPoint(new Vector3(cen.x+ext.x, cen.y+ext.y, cen.z+ext.z))
           };
        Vector2 min = extentPoints[0];
        Vector2 max = extentPoints[0];
        foreach (Vector2 v in extentPoints)
        {
            min = Vector2.Min(min, v);
            max = Vector2.Max(max, v);
        }
        return new Rect(min.x, min.y, max.x - min.x, max.y - min.y);
    }
    public static Vector2 WorldToGUIPoint(Vector3 world)
    {
        Vector2 screenPoint = Camera.main.WorldToScreenPoint(world);
        screenPoint.y = (float)Screen.height - screenPoint.y;
        return screenPoint;
    }
    private void OnGUI()
    {        
        for (int i = 0; i < recs.Length; i++) {
            GUIContent content = new GUIContent(names[i],"");
            GUIStyle style = new GUIStyle();
            style.normal.background = MakeTex((int)recs[i].width, (int)recs[i].height, cs[i]);        
            style.contentOffset = new Vector2(-8, -50);
            style.fontSize = 50;
            GUI.Box(recs[i], content, style);
        }
    }

    private Texture2D MakeTex(int width, int height, Color c)
    {
        Color[] pix = new Color[width * height];
        for (int i = 0; i < pix.Length; ++i)
        {
            int row = i / width;
            int col = i % width;

            if(row <= 5  || row >= height-5 || col  <= 5  || col >= width-5)
                pix[i] = c;
            else
                pix[i] = new Color(0,0,0,0);
        }
        Texture2D result = new Texture2D(width, height);
        result.SetPixels(pix);
        result.Apply();
        return result;
    }

}
