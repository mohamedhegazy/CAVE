import java.awt.Canvas;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.image.BufferedImage;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

import javax.imageio.ImageIO;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;

public class YUVPlayer extends JPanel {
	/**
	 * 
	 */
	private static final long serialVersionUID = 8590930487639879477L;
	static int width = 600;
	static int height = 800;
	static JFrame frame = new JFrame("YUVPlayer");
	static JMenuBar menuBar = new JMenuBar();
	static JMenu fileMenu = new JMenu("File");
	static JMenuItem fileSubMenuItem = new JMenuItem("Open");
	static JButton confirm = new JButton("Finish");
	static JButton replicateLast = new JButton("Replicate Last File");
	static Canvas canvas;
	static File file;
	static String folder;
	static YUVImage img;
	static YUVFile yuv;
	static BufferedImage display;
	static final long ROI_UPDATE_STEP = 2;//update ROI each 15 frames
	static YUVPlayer player = new YUVPlayer();
	int x, y, x2, y2;
	static File prevFile;
	static volatile boolean confirmed = false;	
	static boolean enableMouse = false;
	static ArrayList<Box> boxes = new ArrayList<Box>();
	static Object[] categories = new Object[] { "Player", "Enemy", "Weapon", "Health Pack", "Information/Map" };
	static long counter = 0;
	public void setStartPoint(int x, int y) {
		this.x = x;
		this.y = y;
	}

	public void setEndPoint(int x, int y) {
		x2 = (x);
		y2 = (y);
	}

	 
	private static void copyFileUsingFileStreams(File source, File dest)
			throws IOException {
		InputStream input = null;
		OutputStream output = null;
		try {
			input = new FileInputStream(source);
			output = new FileOutputStream(dest);
			byte[] buf = new byte[1024];
			int bytesRead;
			while ((bytesRead = input.read(buf)) > 0) {
				output.write(buf, 0, bytesRead);
			}
		} finally {
			input.close();
			output.close();
		}
	}
	class MyMouseListener extends MouseAdapter {

		public void mousePressed(MouseEvent e) {
			if(!enableMouse)
				return;
			setStartPoint(e.getX(), e.getY());
		}

		public void mouseDragged(MouseEvent e) {
			if(!enableMouse)
				return;
			setEndPoint(e.getX(), e.getY());
			player.update(player.getGraphics());
		}

		public void mouseReleased(MouseEvent e) {
			if(!enableMouse)
				return;
			setEndPoint(e.getX(), e.getY());
			repaint();
			Object ret = JOptionPane.showInputDialog(null, "Choose object category", "Object Category", JOptionPane.QUESTION_MESSAGE,
					null, categories, categories[0]);
			if(ret != null){
				int i=0;
				for(;i<categories.length;i++){
					if(categories[i]==ret)
						break;
				}
				boxes.add(new Box(Math.min(x, x2), Math.min(y, y2), Math.abs(x - x2), Math.abs(y - y2),i,width,height));
			}
		}
	}

	public YUVPlayer() {
		x = y = x2 = y2 = 0; //
		MyMouseListener listener = new MyMouseListener();
		addMouseListener(listener);
		addMouseMotionListener(listener);
	}

	public void drawPerfectRect(Graphics g, int x, int y, int x2, int y2) {
		int px = Math.min(x, x2);
		int py = Math.min(y, y2);
		int pw = Math.abs(x - x2);
		int ph = Math.abs(y - y2);
		g.drawRect(px, py, pw, ph);
	}

	public void paint(Graphics g) {
		super.paintComponent(g);
		if (display != null) {	
			g.drawImage(display, 0, 0, this);
			if(enableMouse){
				g.setColor(Color.RED);
				drawPerfectRect(g, x, y, x2, y2);
			}			
						
		}
	}

	public void update(Graphics g) {
		paint(g);
	}

	@Override
	public Dimension getPreferredSize() {
		if (isPreferredSizeSet()) {
			return super.getPreferredSize();
		}
		return new Dimension(width, height);
	}

	public static void main(String[] args) {
		SwingUtilities.invokeLater(new Runnable() {
			public void run() {
				createAndShowGui();
			}
		});
	}

	protected static void createAndShowGui() {
		frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		final JFileChooser fc = new JFileChooser();
		menuBar.add(fileMenu);
		fileMenu.add(fileSubMenuItem);
		frame.getContentPane().add(player);
		fileSubMenuItem.addActionListener(new ActionListener() {
			@SuppressWarnings("deprecation")
			@Override
			public void actionPerformed(ActionEvent arg0) {
				int returnVal = fc.showOpenDialog(fileSubMenuItem);

				if (returnVal == JFileChooser.APPROVE_OPTION) {
					file = fc.getSelectedFile();	
					folder = file.getParent();
					String fileName = file.getName().substring(0, file.getName().lastIndexOf('.'));
					String[] parts = fileName.split("_");
					width = Integer.parseInt(parts[1]);
					height = Integer.parseInt(parts[2]);
					player.resize(new Dimension(width, height));
					frame.pack();
					display = new BufferedImage(width, height, BufferedImage.TYPE_INT_RGB);
					Thread thread = new Thread(){
					    public void run(){
					    	handleFrames();
					    	}
					  };
					  thread.start();					  
				}
			}
		});
		replicateLast.setVisible(false);
		replicateLast.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				
				try {
					copyFileUsingFileStreams(prevFile,new File(folder==null?"":folder+"\\roi"+(counter/ROI_UPDATE_STEP)+".txt"));
					confirmed = true;
					confirm.setVisible(false);
					replicateLast.setVisible(false);
					enableMouse = false;
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
				
			}
		});
		
		confirm.setVisible(false);	
		confirm.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				
				try {
					BufferedWriter writer = new BufferedWriter(new FileWriter(folder==null?"":folder+"\\roi"+(counter/ROI_UPDATE_STEP)+".txt"));
					for (int i = 0; i < boxes.size(); i++) {
						Box temp = boxes.get(i);
						writer.write(temp.category+" "+temp.x+" "+temp.y+" "+temp.w+" "+temp.h+"\n");
					}
					File outputfile = new File(folder+"\\frame"+(counter/ROI_UPDATE_STEP)+".jpg");
					ImageIO.write(display, "jpg", outputfile);
					boxes.clear();
					writer.close();
					prevFile = new File(folder==null?"":folder+"\\roi"+(counter/ROI_UPDATE_STEP)+".txt");
				} catch (IOException e) {
					e.printStackTrace();
				}
				confirmed = true;
				confirm.setVisible(false);
				replicateLast.setVisible(false);
				enableMouse = false;
			}
		});
		
		confirm.setPreferredSize(new Dimension(50,10));
		replicateLast.setPreferredSize(new Dimension(50,10));
		menuBar.add(confirm);		
		menuBar.add(replicateLast);
		frame.setResizable(true);
		frame.setJMenuBar(menuBar);
		frame.pack();
		frame.setVisible(true);
	}

	protected static void handleFrames() {
		img = new YUVImage(YUVImage.Format.YUV_420, width, height);
		yuv = new YUVFile(file, img);
		long frameNum = yuv.getFrameNum();
		counter = 0;
		int[] pixels = new int[width * height];
		yuv.read(counter, pixels);
		display.getRaster().setDataElements(0, 0, width, height, pixels);
		player.update(player.getGraphics());
		long startTime = System.currentTimeMillis();
		long elapsedTime = 0L;
		while (counter < frameNum) {
			if (!confirmed && counter % ROI_UPDATE_STEP == 0) {				
				confirm.setVisible(true);
				replicateLast.setVisible(true);
				enableMouse = true;
				while(!confirmed);
			}	
			else{
				confirmed = false;
				counter++;
				yuv.read(counter, pixels);				
				while (elapsedTime < 1000 / ROI_UPDATE_STEP) {
					elapsedTime = System.currentTimeMillis() - startTime;
				}
				elapsedTime = 0L;
				startTime = System.currentTimeMillis();
				display.getRaster().setDataElements(0, 0, width, height, pixels);	
				player.update(player.getGraphics());
			}	
		}
	}
}
