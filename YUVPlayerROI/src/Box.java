
public class Box {
	float x;
	float y;
	float w;
	float h;
	int category;
	public Box(int x, int y, int w, int h,int category, int width, int height) {
		this.x = (float) (1.0*(x+w/2)/width);
		this.y = (float) (1.0*(y+h/2)/height);
		this.w = (float) (1.0*w/width);
		this.h = (float) (1.0*h/height);
		this.category = category;
	}

}
