using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class cameraMovement : MonoBehaviour {
    public GameObject player;
    private Vector3 offset;

    // Use this for initialization
    void Start () {
        offset = transform.position;

    }
    void LateUpdate()
    {
        transform.position = player.transform.position + offset;
    }
    // Update is called once per frame
    void Update () {
		
	}
}
