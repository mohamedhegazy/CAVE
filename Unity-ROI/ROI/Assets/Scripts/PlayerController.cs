using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class PlayerController : MonoBehaviour {
    public float speed;
    private int count;
    public GUIText countText;
    private void Start()
    {
        count = 0;
        CountText();

    }
    void CountText()
    {
        countText.text = "Score: " + count.ToString();
        countText.color = Color.white;
    }

    void OnTriggerEnter(Collider other)
    {
        if (other.gameObject.tag == "item")
	{
            other.gameObject.SetActive(false);
            count = count + 1;
            CountText();
        }        
    }

    void FixedUpdate()
    {
        float moveHorizontal = Input.GetAxis("Horizontal");
        float moveVertical = Input.GetAxis("Vertical");
        Vector3 movement = new Vector3(moveHorizontal, 0.0f, moveVertical);
        GetComponent<Rigidbody>().AddForce(movement * speed * Time.deltaTime);


    }

}
